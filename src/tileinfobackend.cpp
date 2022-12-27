#include "scene/scene.h"

#include "tileinfobackend.h"

TileInfoBackend::TileInfoBackend()
{
}

TileFactoryWrapper *TileInfoBackend::tileFactory() const
{
    return m_tileFactory;
}

void TileInfoBackend::setTileFactory(TileFactoryWrapper *newTileFactory)
{
    if (m_tileFactory == newTileFactory)
        return;

    if (m_tileFactory) {
        disconnect(m_tileFactory, nullptr, this, nullptr);
    }

    m_tileFactory = newTileFactory;
    emit tileFactoryChanged();

    if (m_tileFactory) {
        connect(m_tileFactory, &TileFactoryWrapper::tileDataChanged,
                this, &TileInfoBackend::tileDataChanged);
    }
}

void TileInfoBackend::setTileRef(const QVariantMap &tileRef)
{
    if (m_tileRef == tileRef) {
        return;
    }

    m_tileRef = tileRef;
    QString newTileId;

    if (m_tileRef.contains("tileId")) {
        newTileId = m_tileRef["tileId"].toString();
    }

    if (m_tileId != newTileId) {
        m_tileId = newTileId;
        emit tileIdChanged();
    }

    update();
}

void TileInfoBackend::update()
{
    if (m_tileRef.isEmpty()) {
        m_chartInfos.clear();
        emit chartInfosChanged();
        return;
    }

    if (!m_tileFactory) {
        return;
    }

    std::optional<TileFactoryWrapper::TileRecipe> tileRecipe = Scene::parseTileRef(m_tileRef);

    if (!tileRecipe.has_value()) {
        return;
    }

    m_chartInfos = m_tileFactory->chartInfos(tileRecipe.value());
    emit chartInfosChanged();
}

void TileInfoBackend::setChartEnabled(const QString &chart, bool enabled)
{
    QStringList disabledCharts;

    for (const TileFactory::ChartInfo &chartInfo : m_chartInfos) {
        if (!chartInfo.enabled) {
            disabledCharts.append(QString::fromStdString(chartInfo.name));
        }
    }

    if (enabled && disabledCharts.contains(chart)) {
        disabledCharts.removeAll(chart);
    }

    if (!enabled && !disabledCharts.contains(chart)) {
        disabledCharts.append(chart);
    }

    TileFactory::TileSettings tileSettings;

    for (const QString &disabledChart : disabledCharts) {
        tileSettings.disabledCharts.push_back(disabledChart.toStdString());
    }

    m_tileFactory->setTileSettings(m_tileId.toStdString(), tileSettings);
}

const QVariantMap &TileInfoBackend::tileRef() const
{
    return m_tileRef;
}

QVariantList TileInfoBackend::chartInfos() const
{
    QVariantList output;

    for (const TileFactory::ChartInfo &chartInfo : m_chartInfos) {
        QVariantMap entry;
        entry["name"] = QString::fromStdString(chartInfo.name);
        entry["enabled"] = chartInfo.enabled;
        output.append(entry);
    }

    return output;
}

void TileInfoBackend::tileDataChanged(const QStringList &tileIds)
{
    update();
}

const QString &TileInfoBackend::tileId() const
{
    return m_tileId;
}
