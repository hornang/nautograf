#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#define _USE_MATH_DEFINES
#include <cmath>

#include "tilefactory/georect.h"
#include "tilefactory/itilesource.h"
#include "tilefactory/mercator.h"
#include "tilefactory/tilefactory.h"

static constexpr int maxTileSize = 1024;

std::string TileFactory::tileId(const GeoRect &rectBox, int pixelsPerLongitude)
{
    std::stringstream ss;
    ss << rectBox.top();
    ss << rectBox.left();
    ss << rectBox.bottom();
    ss << rectBox.right();
    ss << pixelsPerLongitude;
    size_t hash = std::hash<std::string> {}(ss.str());

    static const std::string_view hex_chars = "0123456789abcdef";

    std::mt19937 mt(static_cast<unsigned int>(hash));

    std::string uuid;
    unsigned int len = 16;
    uuid.reserve(len);

    while (uuid.size() < len) {
        auto n = mt();
        for (auto i = std::mt19937::max(); i & 0x8 && uuid.size() < len; i >>= 4) {
            uuid += hex_chars[n & 0xf];
            n >>= 4;
        }
    }

    return uuid;
}

void TileFactory::clear()
{
    m_sourcesMutex.lock();
    m_sources.clear();
    m_sourcesMutex.unlock();
}

void TileFactory::setChartEnabled(const std::string &name, bool enabled)
{
    for (auto &source : m_sources) {
        if (source.name == name) {
            if (source.enabled == enabled) {
                return;
            }
            source.enabled = enabled;
            if (m_chartsChangedCb) {
                m_chartsChangedCb({ source.tileSource->extent() });
            }
        }
    }
}

std::vector<int> TileFactory::setAllChartsEnabled(bool enabled)
{
    std::vector<GeoRect> rects;
    std::vector<int> indexes;

    int i = 0;
    for (auto &source : m_sources) {
        if (source.enabled != enabled) {
            source.enabled = enabled;
            rects.push_back(source.tileSource->extent());
            indexes.push_back(i);
            i++;
        }
    }

    if (m_chartsChangedCb) {
        m_chartsChangedCb(rects);
    }
    return indexes;
}

std::vector<std::shared_ptr<Chart>> TileFactory::tileData(const GeoRect &rect,
                                                          double pixelsPerLongitude)
{
    std::vector<std::shared_ptr<Chart>> chartDatas;

    m_sourcesMutex.lock();
    auto sources = m_sources;
    m_sourcesMutex.unlock();

    for (const auto &source : sources) {
        if (!source.enabled) {
            continue;
        }

        int scaleActual = 52246 / (pixelsPerLongitude / 2560 * 0.6);

        const std::shared_ptr<ITileSource> &tileSource = source.tileSource;

        if (!tileSource->extent().intersects(rect)) {
            continue;
        }

        // Avoid showing too detailed maps when zoomed out
        if (scaleActual / 4 > tileSource->scale()) {
            continue;
        }

        std::shared_ptr<Chart> tileData = tileSource->create(rect, pixelsPerLongitude);

        if (!tileData) {
            std::cerr << "No tile data created" << std::endl;
            continue;
        }

        // If we found a map that covers the entire tile we can break here
        if (tileData->coverageType() == ChartData::CoverageType::FULL) {
            chartDatas.push_back(tileData);
            break;
        }

        if (tileData->coverageType() != ChartData::CoverageType::ZERO) {
            chartDatas.push_back(tileData);
        }
    }

    return std::vector<std::shared_ptr<Chart>>(chartDatas.rbegin(), chartDatas.rend());
}

std::vector<TileFactory::Tile> TileFactory::tiles(const Pos &topLeft,
                                                  float pixelsPerLongitude,
                                                  int width,
                                                  int height)
{
    double right = Mercator::mercatorWidthInverse(topLeft.lon(), width, pixelsPerLongitude);
    double bottom = Mercator::mercatorHeightInverse(topLeft.lat(), height, pixelsPerLongitude);
    const GeoRect viewport(topLeft.lat(), bottom, topLeft.lon(), right);
    double logArg = 360 * pixelsPerLongitude / static_cast<double>(maxTileSize);
    double zoom = ceil(log2(logArg));
    zoom = std::clamp<double>(zoom, 0, 23);
    int maxPixelsPerLon = maxTileSize / 360. * pow(2, zoom);
    std::vector<GeoRect> tileLocations = tilesInViewport(viewport, zoom);

    if (m_previousTileLocations == tileLocations) {
        return m_previousTiles;
    }
    m_previousTileLocations = tileLocations;

    std::vector<TileFactory::Tile> tiles;

    for (const auto &tileRect : tileLocations) {
        for (const auto &source : m_sources) {
            if (tileRect.intersects(source.tileSource->extent())) {
                Tile tile { tileId(tileRect, maxPixelsPerLon),
                            tileRect,
                            maxPixelsPerLon };
                tiles.push_back(tile);
                break;
            }
        }
    }

    m_previousTiles = tiles;
    return tiles;
}

std::vector<GeoRect> TileFactory::tilesInViewport(const GeoRect &rect, int zoom)
{
    assert(zoom >= 0);

    double topLat = rect.top() * M_PI / 180.;
    double bottomLat = rect.bottom() * M_PI / 180.;

    double yTop = (M_PI - log(tan(M_PI / 4 + topLat / 2))) / (2 * M_PI);
    double yBottom = (M_PI - log(tan(M_PI / 4 + bottomLat / 2))) / (2 * M_PI);

    double xLeft = (rect.left() + 180.) / 360.;
    double xRight = (rect.right() + 180.) / 360.;

    const int totalVerticalTiles = pow(2, zoom);
    const int startVerticalTile = floor(totalVerticalTiles * yTop);
    const int endVerticalTile = ceil(totalVerticalTiles * yBottom);

    const int totalHorizontalTiles = pow(2, zoom);
    const int startHorizontalTile = floor(totalHorizontalTiles * xLeft);
    const int endHorizontalTile = ceil(totalHorizontalTiles * xRight);

    std::vector<GeoRect> tiles;

    for (int yTile = startVerticalTile; yTile < endVerticalTile; yTile++) {
        const double normalizedTop = static_cast<double>(yTile) / totalVerticalTiles;
        const double normalizedBottom = static_cast<double>(yTile + 1) / totalVerticalTiles;
        const double topLat = Mercator::toLatitude(normalizedTop);
        const double bottomLat = Mercator::toLatitude(normalizedBottom);

        for (int xTile = startHorizontalTile; xTile < endHorizontalTile; xTile++) {
            const double normalizedLeft = static_cast<double>(xTile) / totalHorizontalTiles;
            const double normalizedRight = static_cast<double>(xTile + 1) / totalHorizontalTiles;
            const double leftLon = Mercator::toLongitude(normalizedLeft);
            const double rightLon = Mercator::toLongitude(normalizedRight);
            tiles.push_back(GeoRect(topLat, bottomLat, leftLon, rightLon));
        }
    }
    return tiles;
}

bool TileFactory::hasSource(const std::string &name) const
{
    for (const auto &source : m_sources) {
        if (source.name == name) {
            return true;
        }
    }
    return false;
}

void TileFactory::insertSorted(const TileFactory::Source &source)
{

    auto upper = std::upper_bound(m_sources.begin(),
                                  m_sources.end(),
                                  source, [](const TileFactory::Source &a, const TileFactory::Source &b) -> bool {
                                      return a.tileSource->scale() < b.tileSource->scale();
                                  });

    m_sources.insert(upper, source);
}

void TileFactory::addSources(const std::vector<TileFactory::Source> &sources)
{
    std::vector<GeoRect> rois;

    for (const auto &source : sources) {
        if (!source.tileSource) {
            std::cerr << "Not adding null source" << std::endl;
            continue;
        }
        if (hasSource(source.name)) {
            std::cerr << "Source with name " << source.name << " already exists " << std::endl;
            continue;
        }
        rois.push_back(source.tileSource->extent());
        insertSorted(source);
    }

    m_previousTileLocations.clear();

    if (m_updateCallback) {
        m_updateCallback();
    }

    if (m_chartsChangedCb) {
        m_chartsChangedCb(rois);
    }
}
