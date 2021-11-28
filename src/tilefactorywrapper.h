#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <QObject>

#include "tilefactory/georect.h"

class Chart;

class TileFactoryWrapper : public QObject
{
    Q_OBJECT

public:
    struct TileRecipe
    {
        GeoRect rect;
        double pixelsPerLongitude;
    };

    using TileDataCallback = std::function<std::vector<std::shared_ptr<Chart>>(GeoRect, double)>;

    void setTileDataCallback(TileDataCallback callback);
    std::vector<std::shared_ptr<Chart>> create(TileRecipe recipe);

private:
    TileDataCallback m_tileDataCallback;
};
