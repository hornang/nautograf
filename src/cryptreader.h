#pragma once

#include <QLocalSocket>
#include <QObject>
#include <QProcess>

class CryptReader : public QObject
{
    Q_OBJECT
public:
    CryptReader();
    ~CryptReader();
    void start();
    void read(const QString &fileName, const QString &key);
    bool ready() const;

public slots:
    void stateChanged(QLocalSocket::LocalSocketState socketState);
    void oeserverdStarted();

signals:
    void readyChanged(bool ready);
    void fileDecrypted(const QByteArray &data);
    void error();

private:
    void testPipe();
    bool readFile(const QString &fileName, const QString &key);

    enum class OesencServiceCommand {
        ReadChart = 0,
        TestAvailability = 1,
        Exit = 2,
        ReadChartHeader = 3,
    };

    enum class State {
        Unknown,
        Ready,
        WaitForReading
    };

    QProcess m_oeserverd;

    void connectToPipe();
    void startOeserverd();
    static void add256String(QByteArray &data, const QString &string);
    static QByteArray constructMessage(CryptReader::OesencServiceCommand command,
                                       const QString &socket,
                                       const QString &filename,
                                       const QString &key);

    State m_state = State::Unknown;
    bool m_ready = false;
    QByteArray m_receivedData;
    QString m_fileName;
    QString m_key;
    QLocalSocket m_localSocket;
};
