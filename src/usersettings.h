#pragma once

#include <QObject>
#include <QVector3D>
#include <QWindow>

static const QString orgName = QStringLiteral("Seatronomy");
static const QString appName = QStringLiteral("Nautograf");

class UserSettings : public QObject
{
    Q_PROPERTY(QVector3D viewport READ viewport WRITE setViewPort NOTIFY viewportChanged)
    Q_PROPERTY(QWindow::Visibility windowState READ windowState WRITE setWindowState NOTIFY windowStateChanged)
    Q_PROPERTY(QRect geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)
    Q_PROPERTY(bool debugView READ debugView WRITE setDebugView NOTIFY debugViewChanged)
    Q_OBJECT

public:
    UserSettings();
    ~UserSettings();
    const QVector3D &viewport() const;
    void setViewPort(const QVector3D &newViewport);
    QWindow::Visibility windowState() const;
    void setWindowState(QWindow::Visibility newWindowState);
    const QRect &geometry() const;
    void setGeometry(const QRect &newGeometry);

    bool debugView() const;
    void setDebugView(bool newDebugView);

private:
    QVector3D m_viewport = QVector3D((float)4.5, (float)61, (float)200);
    void write();
    void read();
    QWindow::Visibility m_windowState = QWindow::Windowed;
    QRect m_geometry = QRect(200, 200, 1280, 1024);
    bool m_debugView;

signals:
    void viewportChanged();
    void windowStateChanged();
    void geometryChanged();
    void debugViewChanged();
};
