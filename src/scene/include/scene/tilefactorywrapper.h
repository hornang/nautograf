#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <QObject>

#include "tilefactory/georect.h"
#include "tilefactory/tilefactory.h"

#include "scene_export.h"

class Chart;

class SCENE_EXPORT TileFactoryWrapper : public QObject
{
    Q_OBJECT

public:
    struct TileRecipe
    {
        GeoRect rect;
        double pixelsPerLongitude;
    };

    using TileDataCallback = std::function<std::vector<std::shared_ptr<Chart>>(GeoRect, double)>;
    using ChartInfoCallback = std::function<std::vector<TileFactory::ChartInfo>(GeoRect, double)>;
    using TileSettingsCallback = std::function<void(const std::string &tileId, TileFactory::TileSettings)>;

    void setTileSettings(const std::string &tileId, TileFactory::TileSettings tileSettings);
    void setTileDataCallback(TileDataCallback callback);
    void setChartInfoCallback(ChartInfoCallback callback);
    void setTileSettingsCb(TileSettingsCallback callback);
    std::vector<std::shared_ptr<Chart>> create(TileRecipe recipe);
    std::vector<TileFactory::ChartInfo> chartInfos(TileRecipe recipe);

    void triggerTileDataChanged(const std::vector<std::string> &tileIds);

signals:
    void tileDataChanged(QStringList tileIds);

private:
    TileDataCallback m_tileDataCallback;
    ChartInfoCallback m_chartInfoCallback;
    TileSettingsCallback m_tileSettingsCallback;
};
