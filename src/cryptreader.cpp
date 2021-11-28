#include <chrono>

#include <QLocalSocket>
#include <QTimer>

#include "cryptreader.h"

using namespace std::chrono_literals;

static const QString pipeName = QStringLiteral("ocpn_pipe");
static const QString windowsSocketFormat = QStringLiteral(R"(\\.\pipe\%1)");
static const QString oeserverdPathWindows = QStringLiteral(R"(C:\Program Files (x86)\OpenCPN\plugins\oesenc_pi\oeserverd.exe)");
static const std::chrono::duration waitConnectTime = 200ms;

CryptReader::CryptReader()
    : m_oeserverd()
{
    connect(&m_localSocket, &QLocalSocket::stateChanged, this, &CryptReader::stateChanged, Qt::QueuedConnection);
    connect(&m_oeserverd, &QProcess::started, this, &CryptReader::oeserverdStarted);
    connect(&m_oeserverd, &QProcess::errorOccurred, this, &CryptReader::error);
}

void CryptReader::start()
{
    connectToPipe();
}

void CryptReader::testPipe()
{
    connectToPipe();
}

CryptReader::~CryptReader()
{
    connectToPipe();
    m_localSocket.waitForConnected();

    if (m_localSocket.isOpen()) {
        QByteArray message;
        char command = static_cast<char>(CryptReader::OesencServiceCommand::Exit);
        message.append(static_cast<char>(command));
        add256String(message, QString());
        add256String(message, QString());
        add256String(message, QString());
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

    qInfo() << "Starting oeserverd";

    m_oeserverd.setProgram(oeserverdPathWindows);
    m_oeserverd.setArguments({ "-p", pipeName });
    m_oeserverd.start();
}

void CryptReader::oeserverdStarted()
{
    QTimer::singleShot(waitConnectTime, this, [&]() { testPipe(); });
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

QByteArray CryptReader::constructMessage(CryptReader::OesencServiceCommand command,
                                         const QString &socket,
                                         const QString &filename,
                                         const QString &key)
{
    QByteArray message;
    message.append(static_cast<char>(command));
    add256String(message, socket);
    add256String(message, filename);
    add256String(message, key);
    return message;
}

void CryptReader::read(const QString &fileName, const QString &key)
{
    if (m_state != State::Ready) {
        qWarning() << "Not ready to read";
        return;
    }
    m_state = State::WaitForReading;
    m_fileName = fileName;
    m_key = key;

    connectToPipe();
}

bool CryptReader::ready() const
{
    return m_ready;
}

bool CryptReader::readFile(const QString &fileName, const QString &key)
{
    if (fileName.isEmpty()) {
        qCritical() << "Trying to read with zero filename";
        return false;
    }

    if (m_localSocket.state() != QLocalSocket::ConnectedState) {
        qCritical() << "Localsocket not connected";
        return false;
    }

    const QByteArray message = constructMessage(CryptReader::OesencServiceCommand::ReadChart,
                                                QString(),
                                                fileName,
                                                key);

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
        if (m_state == State::Unknown) {
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
        if (m_state == State::Unknown) {
            m_state = State::Ready;
            m_localSocket.close();
        } else if (m_state == State::WaitForReading) {
            readFile(m_fileName, m_key);
        }
    }
}

void CryptReader::connectToPipe()
{
    if (m_localSocket.state() == QLocalSocket::LocalSocketState::ConnectedState) {
        return;
    }
    m_localSocket.connectToServer(windowsSocketFormat.arg(pipeName));
}
