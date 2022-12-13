#include "tilefactorywrapper.h"

std::vector<std::shared_ptr<Chart>> TileFactoryWrapper::create(TileRecipe recipe)
{
    return m_tileDataCallback(recipe.rect, recipe.pixelsPerLongitude);
}

void TileFactoryWrapper::setTileDataCallback(TileDataCallback tileDataCallback)
{
    m_tileDataCallback = tileDataCallback;
}

std::vector<TileFactory::ChartInfo> TileFactoryWrapper::chartInfos(TileRecipe recipe)
{
    assert(m_chartInfoCallback);
    return m_chartInfoCallback(recipe.rect, recipe.pixelsPerLongitude);
}

void TileFactoryWrapper::setChartInfoCallback(ChartInfoCallback callback)
{
    m_chartInfoCallback = callback;
}

void TileFactoryWrapper::triggerTileDataChanged(const std::vector<std::string> &_tileIds)
{
    QStringList tileIds;

    for (const std::string &tileId : _tileIds) {
        tileIds.push_back(QString::fromStdString(tileId));
    }
    emit tileDataChanged(tileIds);
}

void TileFactoryWrapper::setTileSettingsCb(TileSettingsCallback callback)
{
    m_tileSettingsCallback = callback;
}

void TileFactoryWrapper::setTileSettings(const std::string &tileId, TileFactory::TileSettings tileSettings)
{
    assert(m_tileSettingsCallback);

    m_tileSettingsCallback(tileId, tileSettings);
}
