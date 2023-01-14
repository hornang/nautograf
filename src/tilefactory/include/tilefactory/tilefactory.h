#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "itilesource.h"
#include "tilefactory/georect.h"
#include "tilefactory/pos.h"

#include "tilefactory_export.h"

class TILEFACTORY_EXPORT TileFactory
{
public:
    TileFactory() = default;

    struct Tile
    {
        std::string tileId;
        GeoRect boundingBox;
        int maxPixelsPerLon;
    };

    struct ChartInfo
    {
        std::string name;
        GeoRect boundingBox;
        bool enabled;
    };

    struct TileSettings
    {
        std::vector<std::string> disabledCharts;
    };

    /*!
        Helper structure to add enable property to hide certain charts from
        rendering even if they are loaded.
    */
    struct Source
    {
        std::string name;
        std::shared_ptr<ITileSource> tileSource;
        bool enabled = true;
    };

    /*!
        Returns the tiles within the given viewport that intersects with the
        loaded charts.

        \param topLeft The geodetic position of the viewport's top left corner.
        \param pixelsPerLon Pixels per longitude for the viewport.
        \param width Width of viewport in pixels.
        \param height Height of viewport in pixels.
    */
    std::vector<Tile> tiles(const Pos &topLeft, float pixelsPerLon, int width, int height);

    /*!
        Removes all chart sources.

        This function may block if a source is currently beeing used by another
        thread through \ref tileData.
    */
    void clear();

    /*!
        Returns a list of tile data for the given tile geo rectangle.

        This will trigger creation of the data if it is not already cached to
        disk. Therefore the function could take some time before it returns.
    */
    std::vector<std::shared_ptr<Chart>> tileData(const GeoRect &rect, double pixelsPerLongitude);
    std::vector<TileFactory::ChartInfo> chartInfo(const GeoRect &rect, double pixelsPerLongitude);

    void setUpdateCallback(std::function<void(void)> updateCallback) { m_updateCallback = updateCallback; }
    void setChartsChangedCb(std::function<void(std::vector<GeoRect> roi)> chartsChangedCb) { m_chartsChangedCb = chartsChangedCb; }
    void setSources(const std::vector<TileFactory::Source> &sources);

    using TileDataChangedCallback = std::function<void(std::vector<std::string>)>;
    void setTileDataChangedCallback(TileDataChangedCallback tileDataChangedCallback) { m_tileDataChangedCallback = tileDataChangedCallback; }
    void setTileSettings(const std::string &tileId, TileSettings tileSettings);
    void setChartEnabled(const std::string &name, bool enabled);
    std::vector<int> setAllChartsEnabled(bool enabled);
    std::vector<Source> sources() const { return m_sources; }

private:
    std::vector<Source> sourceCandidates(const GeoRect &rect, double pixelsPerLon);
    bool chartEnabledForTile(const std::string &chart, const std::string &tileId) const;
    bool hasSource(const std::string &id);
    static std::vector<GeoRect> tilesInViewport(const GeoRect &rect, int zoom);
    std::function<void(void)> m_updateCallback;
    std::function<void(std::vector<GeoRect> roi)> m_chartsChangedCb;
    TileDataChangedCallback m_tileDataChangedCallback;
    std::vector<Source> m_sources;
    std::mutex m_sourcesMutex;
    std::vector<GeoRect> m_previousTileLocations;
    std::vector<TileFactory::Tile> m_previousTiles;
    std::unordered_map<std::string, TileSettings> m_tileSettings;
};
