#include <chrono>

#include <QDir>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QTimer>

#include "cryptreader.h"

using namespace std::chrono_literals;

namespace {
const QString pipeName = QStringLiteral("ocpn_pipe");
const QString windowsSocketFormat = QStringLiteral(R"(\\.\pipe\%1)");
const std::chrono::duration connectDelay = 200ms;
const std::chrono::duration retryDelay = 500ms;
const int maxConnectionAttempts = 5;
}

CryptReader::CryptReader()
    : m_oeserverd()
{
    connect(&m_localSocket, &QLocalSocket::stateChanged, this, &CryptReader::stateChanged, Qt::QueuedConnection);
    connect(&m_localSocket, &QLocalSocket::errorOccurred, this, [&]() {
        m_state = State::ServiceNotResponding;
        updateStatusString();

        if (!m_pendingConnectAttempt) {
            m_pendingConnectAttempt = true;
            if (m_connectionAttemptNo < maxConnectionAttempts) {
                QTimer::singleShot(retryDelay, this, [&]() {
                    m_pendingConnectAttempt = false;
                    connectToPipe();
                });
                m_connectionAttemptNo++;
            } else {
                qWarning() << "Failed to connect after " << maxConnectionAttempts;
            }
        }
    });
    connect(&m_oeserverd, &QProcess::started, this, &CryptReader::oeserverdStarted);
    connect(&m_oeserverd, &QProcess::errorOccurred, this, [&](QProcess::ProcessError errorCode) {
        emit error();
        m_processError = errorCode;
        updateStatusString();
    });
}

void CryptReader::start()
{
    connectToPipe();
}

CryptReader::~CryptReader()
{
    m_oeserverd.disconnect();
    connectToPipe();
    m_localSocket.waitForConnected();

    if (m_localSocket.isOpen()) {
        QByteArray message;
        char command = static_cast<char>(CryptReader::OesencServiceCommand::Exit);
        message.append(static_cast<char>(command));
        add256String(message, QString());
        add256String(message, QString());
        add512String(message, QString());
        m_localSocket.write(message);

        if (m_oeserverd.waitForFinished(2000)) {
            return;
        }
    }

    if (m_oeserverd.state() == QProcess::Running) {
        m_oeserverd.kill();
        if (!m_oeserverd.waitForFinished(2000)) {
            qWarning() << "oeserverd not quitting from kill signal";
        }
    }
}

void CryptReader::startOeserverd()
{
    if (m_oeserverd.state() != QProcess::NotRunning) {
        return;
    }

    qInfo() << "Starting oexserverd";
    const auto appDataFolder = QStandardPaths::locate(QStandardPaths::HomeLocation,
                                                      "AppData",
                                                      QStandardPaths::LocateDirectory);

    const auto oexserverdPath = QDir(appDataFolder).filePath("Local/opencpn/plugins/oexserverd.exe");

    if (!QFile::exists(oexserverdPath)) {
        m_state = State::ServiceNotInstalled;
        emit error();
        return;
    }

    m_oeserverd.setProgram(oexserverdPath);
    m_oeserverd.setArguments({ "-p", pipeName });
    m_oeserverd.start();
}

void CryptReader::oeserverdStarted()
{
    QTimer::singleShot(connectDelay, this, [&]() { connectToPipe(); });
}

void CryptReader::add256String(QByteArray &data, const QString &string)
{
    if (string.size() > 256) {
        qWarning() << "Too long string";
        return;
    }

    QByteArray fifoName(256, 0x00);
    fifoName.replace(0, string.size(), string.toLocal8Bit());
    data.push_back(fifoName);
}

void CryptReader::add512String(QByteArray &data, const QString &string)
{
    if (string.size() > 512) {
        qWarning() << "Too long string";
        return;
    }

    QByteArray fifoName(512, 0x00);
    fifoName.replace(0, string.size(), string.toLocal8Bit());
    data.push_back(fifoName);
}

QByteArray CryptReader::constructMessage(CryptReader::OesencServiceCommand command,
                                         const QString &socket,
                                         const QString &filename,
                                         const QString &key)
{
    QByteArray message;
    message.append(static_cast<char>(command));
    add256String(message, socket);
    add256String(message, filename);
    add512String(message, key);
    return message;
}

bool CryptReader::read(const QString &fileName, const QString &key, ChartType type)
{
    if (m_state != State::Ready) {
        qWarning() << "Not ready to read";
        return false;
    }
    m_state = State::WaitForReading;
    m_fileName = fileName;
    m_key = key;
    m_type = type;

    connectToPipe();
    return true;
}

bool CryptReader::ready() const
{
    return m_ready;
}

bool CryptReader::readFile(const QString &fileName, const QString &key, ChartType type)
{
    if (fileName.isEmpty()) {
        qCritical() << "Trying to read with zero filename";
        return false;
    }

    if (m_localSocket.state() != QLocalSocket::ConnectedState) {
        qCritical() << "Localsocket not connected";
        return false;
    }

    CryptReader::OesencServiceCommand command;

    switch (type) {
    case ChartType::Oesenc:
        command = CryptReader::OesencServiceCommand::ReadOesenc;
        break;
    case ChartType::Oesu:
        command = CryptReader::OesencServiceCommand::ReadOesu;
        break;
    default:
        return false;
    }

    const auto message = constructMessage(command, QString(), fileName, key);

    m_localSocket.write(message);
    m_localSocket.waitForBytesWritten();
    m_receivedData.clear();

    while (m_localSocket.waitForReadyRead(2000)) {
        QByteArray newData = m_localSocket.readAll();
        m_receivedData += newData;
    }

    return !m_receivedData.isEmpty();
}

void CryptReader::stateChanged(QLocalSocket::LocalSocketState socketState)
{
    if (socketState == QLocalSocket::LocalSocketState::UnconnectedState) {
        if (m_state == State::Unknown || m_state == State::ServiceNotResponding) {
            startOeserverd();
        } else if (m_state == State::WaitForReading) {
            m_state = State::Ready;
            m_fileName.clear();
            emit fileDecrypted(m_receivedData);
            m_receivedData.clear();
        } else if (m_state == State::Ready) {
            m_ready = true;
            emit readyChanged(true);
        }
    } else if (socketState == QLocalSocket::LocalSocketState::ConnectedState) {
        if (m_state == State::Unknown || m_state == State::ServiceNotResponding) {
            m_state = State::Ready;
            m_localSocket.close();
        } else if (m_state == State::WaitForReading) {
            readFile(m_fileName, m_key, m_type);
        }
    }

    updateStatusString();
}

void CryptReader::updateStatusString()
{
    if (m_processError != QProcess::UnknownError) {
        switch (m_processError) {
        case QProcess::FailedToStart:
            m_statusString = "Failed to start";
            break;
        case QProcess::Crashed:
            m_statusString = "Crashed";
            break;
        default:
            m_statusString = "Unknown error";
            break;
        }
    } else {
        switch (m_state) {
        case State::Unknown:
            m_statusString = "Connecting";
            break;
        case State::ServiceNotInstalled:
            m_statusString = "Not installed";
            break;
        case State::ServiceNotResponding:
            m_statusString = "Unable to connect";
            break;
        case State::Ready:
            m_statusString = "Ready";
            break;
        case State::WaitForReading:
            m_statusString = "Reading";
            break;
        }
    }

    emit statusStringChanged(m_statusString);
}

void CryptReader::connectToPipe()
{
    if (m_localSocket.state() == QLocalSocket::LocalSocketState::ConnectedState) {
        return;
    }

    m_localSocket.connectToServer(windowsSocketFormat.arg(pipeName));
}
