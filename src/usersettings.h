#pragma once

#include <QObject>
#include <QVector3D>
#include <QWindow>

static const QString orgName = QStringLiteral("Seatronomy");
static const QString appName = QStringLiteral("Nautograf");

class UserSettings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double lat READ lat WRITE setLat NOTIFY latChanged)
    Q_PROPERTY(double lon READ lon WRITE setLon NOTIFY lonChanged)
    Q_PROPERTY(double pixelsPerLon READ pixelsPerLon WRITE setPixelsPerLon NOTIFY pixelsPerLonChanged)

    Q_PROPERTY(QWindow::Visibility windowState READ windowState WRITE setWindowState NOTIFY windowStateChanged)
    Q_PROPERTY(QRect geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)
    Q_PROPERTY(bool debugView READ debugView WRITE setDebugView NOTIFY debugViewChanged)
    Q_PROPERTY(bool showLegacyRenderer READ showLegacyRenderer WRITE setLegacyRender NOTIFY showLegacyRendererChanged)

public:
    UserSettings();
    ~UserSettings();
    QWindow::Visibility windowState() const;
    void setWindowState(QWindow::Visibility newWindowState);
    const QRect &geometry() const;
    void setGeometry(const QRect &newGeometry);

    bool debugView() const;
    void setDebugView(bool newDebugView);

    bool showLegacyRenderer() const;
    void setLegacyRender(bool newLegacyRender);

    double lat() const;
    void setLat(const double &newLat);

    double lon() const;
    void setLon(const double &newLon);

    double pixelsPerLon() const;
    void setPixelsPerLon(const double &newPixelsPerLon);

private:
    static const QString showLegacyRendererKey;
    void write();
    void read();
    QWindow::Visibility m_windowState = QWindow::Windowed;
    QRect m_geometry = QRect(200, 200, 1280, 1024);
    double m_lat = 61;
    double m_lon = 4.5;
    double m_pixelsPerLon = 200;
    bool m_debugView;
    bool m_showLegacyRenderer = false;

signals:
    void latChanged();
    void lonChanged();
    void pixelsPerLonChanged();
    void windowStateChanged();
    void geometryChanged();
    void debugViewChanged();
    void showLegacyRendererChanged();
};
