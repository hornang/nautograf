#include <algorithm>

#include <QSettings>

#include "usersettings.h"

const double maxLatFromSettings = 85.0;

static const QString viewportKey = QStringLiteral("Viewport");
const QString UserSettings::showLegacyRendererKey = QStringLiteral("ShowLegacyRenderer");

UserSettings::UserSettings()
{
    read();
}

UserSettings::~UserSettings()
{
    write();
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

    QVariant latValue = settings.value("Latitude");
    QVariant lonValue = settings.value("Longitude");
    QVariant pixelsPerLongitudeValue = settings.value("PixelsPerLongitude");

    if (latValue.isValid() && lonValue.isValid() && pixelsPerLongitudeValue.isValid()) {
        double lon = lonValue.toDouble();
        double lat = latValue.toDouble();
        double pixelsPerLongitude = pixelsPerLongitudeValue.toDouble();

        if (std::isfinite(lon) && std::isfinite(lat) && std::isfinite(pixelsPerLongitude)) {
            lat = std::clamp<double>(lat, -maxLatFromSettings, maxLatFromSettings);
            lon = std::clamp<double>(lon, -180, 180);

            m_lat = lat;
            emit latChanged();
            m_lon = lon;
            emit lonChanged();
            m_pixelsPerLon = pixelsPerLongitude;
            emit pixelsPerLonChanged();
        } else {
            qWarning() << "Invalid viewport" << lat << lon << pixelsPerLongitude;
        }
    }

    bool debugView = settings.value("DebugView").toBool();
    if (m_debugView != debugView) {
        m_debugView = debugView;
        emit debugViewChanged();
    }

    bool showLegacyRenderer = settings.value(showLegacyRendererKey).toBool();
    if (m_showLegacyRenderer != showLegacyRenderer) {
        m_showLegacyRenderer = showLegacyRenderer;
        emit showLegacyRendererChanged();
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
    settings.setValue("Latitude", m_lat);
    settings.setValue("Longitude", m_lon);
    settings.setValue("PixelsPerLongitude", m_pixelsPerLon);
    settings.setValue("DebugView", m_debugView);
    settings.setValue(showLegacyRendererKey, m_showLegacyRenderer);
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

bool UserSettings::showLegacyRenderer() const
{
    return m_showLegacyRenderer;
}

void UserSettings::setLegacyRender(bool newLegacyRender)
{
    if (m_showLegacyRenderer == newLegacyRender)
        return;
    m_showLegacyRenderer = newLegacyRender;
    emit showLegacyRendererChanged();
}

double UserSettings::lat() const
{
    return m_lat;
}

void UserSettings::setLat(const double &newLat)
{
    if (m_lat == newLat)
        return;
    m_lat = newLat;
    emit latChanged();
}

double UserSettings::lon() const
{
    return m_lon;
}

void UserSettings::setLon(const double &newLon)
{
    if (m_lon == newLon)
        return;
    m_lon = newLon;
    emit lonChanged();
}

double UserSettings::pixelsPerLon() const
{
    return m_pixelsPerLon;
}

void UserSettings::setPixelsPerLon(const double &newPixelsPerLon)
{
    if (m_pixelsPerLon == newPixelsPerLon)
        return;
    m_pixelsPerLon = newPixelsPerLon;
    emit pixelsPerLonChanged();
}
