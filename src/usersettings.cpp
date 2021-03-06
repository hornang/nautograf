#include <QSettings>

#include "usersettings.h"

static const QString viewportKey = QStringLiteral("Viewport");

UserSettings::UserSettings()
{
    read();
}

UserSettings::~UserSettings()
{
    write();
}

const QVector3D &UserSettings::viewport() const
{
    return m_viewport;
}

void UserSettings::read()
{
    QSettings settings(orgName, appName);

    auto rectVariant = settings.value("Geometry");
    if (settings.contains("Geometry")) {
        m_geometry = rectVariant.toRect();
        emit geometryChanged();
    }
    auto windowStateVariant = settings.value("WindowState");
    bool ok;
    auto windowState = windowStateVariant.toInt(&ok);
    if (ok) {
        // Not really guaranteed to work if enum changes
        m_windowState = static_cast<QWindow::Visibility>(windowState);
        if (m_windowState == QWindow::Hidden) {
            // Hidden not allowed
            m_windowState = QWindow::Windowed;
        }
        emit windowStateChanged();
    }
    settings.beginGroup(viewportKey);

    auto lat = settings.value("Latitude");
    auto lon = settings.value("Longitude");
    auto pixelsPerLongitude = settings.value("PixelsPerLongitude");

    if (lat.isValid() && lon.isValid() && pixelsPerLongitude.isValid()) {
        m_viewport = QVector3D(lon.toDouble(), lat.toDouble(), pixelsPerLongitude.toDouble());
        emit viewportChanged();
    }

    bool debugView = settings.value("DebugView").toBool();
    if (m_debugView != debugView) {
        m_debugView = debugView;
        emit debugViewChanged();
    }
}

void UserSettings::write()
{
    QSettings settings(orgName, appName);
    if (m_windowState == QWindow::Windowed) {
        settings.setValue("Geometry", m_geometry);
    }
    settings.setValue("WindowState", m_windowState);
    settings.beginGroup(viewportKey);
    settings.setValue("Latitude", m_viewport.y());
    settings.setValue("Longitude", m_viewport.x());
    settings.setValue("PixelsPerLongitude", m_viewport.z());
    settings.setValue("DebugView", m_debugView);
}

void UserSettings::setViewPort(const QVector3D &newViewport)
{
    if (m_viewport == newViewport)
        return;
    m_viewport = newViewport;
    emit viewportChanged();
}

QWindow::Visibility UserSettings::windowState() const
{
    return m_windowState;
}

void UserSettings::setWindowState(QWindow::Visibility newWindowState)
{
    if (m_windowState == newWindowState || newWindowState == QWindow::Hidden)
        return;
    m_windowState = newWindowState;
    emit windowStateChanged();
}

const QRect &UserSettings::geometry() const
{
    return m_geometry;
}

void UserSettings::setGeometry(const QRect &newGeometry)
{
    if (m_geometry == newGeometry)
        return;
    m_geometry = newGeometry;
    emit geometryChanged();
}

bool UserSettings::debugView() const
{
    return m_debugView;
}

void UserSettings::setDebugView(bool newDebugView)
{
    if (m_debugView == newDebugView)
        return;
    m_debugView = newDebugView;
    emit debugViewChanged();
}
