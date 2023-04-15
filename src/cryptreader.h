#pragma once

#include <QLocalSocket>
#include <QObject>
#include <QProcess>

class CryptReader : public QObject
{
    Q_OBJECT
public:
    enum class ChartType {
        Unknown,
        Oesenc,
        Oesu,
    };

    CryptReader();
    ~CryptReader();
    void start();
    bool read(const QString &fileName, const QString &key, ChartType type);
    bool ready() const;
    QString statusString() const;

public slots:
    void stateChanged(QLocalSocket::LocalSocketState socketState);
    void oeserverdStarted();

signals:
    void readyChanged(bool ready);
    void statusStringChanged(const QString &statusString);
    void fileDecrypted(const QByteArray &data);
    void error();

private:
    bool readFile(const QString &fileName, const QString &key, ChartType type);
    void updateStatusString();

    enum class OesencServiceCommand {
        ReadOesenc = 0,
        TestAvailability = 1,
        Exit = 2,
        ReadChartHeader = 3,
        ReadOesu = 8,
    };

    enum class State {
        Unknown,
        ServiceNotInstalled,
        ServiceNotResponding,
        Ready,
        WaitForReading
    };

    QProcess m_oeserverd;

    void connectToPipe();
    void startOeserverd();
    static void add256String(QByteArray &data, const QString &string);
    static void add512String(QByteArray &data, const QString &string);
    static QByteArray constructMessage(CryptReader::OesencServiceCommand command,
                                       const QString &socket,
                                       const QString &filename,
                                       const QString &key);

    State m_state = State::Unknown;
    QString m_statusString;
    bool m_ready = false;
    QByteArray m_receivedData;
    QString m_fileName;
    QString m_key;
    QProcess::ProcessError m_processError = QProcess::UnknownError;
    ChartType m_type = ChartType::Unknown;
    QLocalSocket m_localSocket;
    int m_connectionAttemptNo = 0;
    bool m_pendingConnectAttempt = false;
};
