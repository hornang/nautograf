#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#define _USE_MATH_DEFINES
#include <cmath>

#include <mercatortile/MercatorTile.h>

#include "filehelper.h"
#include "tilefactory/georect.h"
#include "tilefactory/itilesource.h"
#include "tilefactory/mercator.h"
#include "tilefactory/oesenctilesource.h"
#include "tilefactory/tilefactory.h"

#include "coverageratio.h"

namespace {
constexpr int maxTileSize = 1024;
float coverageAccpetanceThreshold = 0.98f;

mercatortile::LngLatBbox convertToMercatorTileBox(const GeoRect &rect)
{
    return { rect.left(), rect.bottom(), rect.right(), rect.top() };
}

GeoRect convertToGeoRect(const mercatortile::LngLatBbox &lngLatBox)
{
    return { lngLatBox.north, lngLatBox.south, lngLatBox.west, lngLatBox.east };
}
}

void TileFactory::clear()
{
    const std::lock_guard<std::mutex> lock(m_sourcesMutex);
    m_sources.clear();
}

void TileFactory::setChartEnabled(const std::string &name, bool enabled)
{
    std::vector<Source> changedSources;

    {
        const std::lock_guard<std::mutex> lock(m_sourcesMutex);

        for (auto &source : m_sources) {
            if (source.name == name) {
                if (source.enabled == enabled) {
                    return;
                }
                source.enabled = enabled;
                changedSources.push_back(source);
                if (m_chartsChangedCb) {
                    m_chartsChangedCb({ source.tileSource->extent() });
                }
            }
        }
    }

    if (!m_tileDataChangedCallback) {
        return;
    }

    std::vector<std::string> tilesAffected;

    for (const Source &source : changedSources) {
        for (const Tile &tile : m_previousTiles) {
            if (source.tileSource->extent().intersects(tile.boundingBox)) {
                tilesAffected.push_back(tile.tileId);
            }
        }
    }

    m_tileDataChangedCallback(tilesAffected);
}

std::vector<int> TileFactory::setAllChartsEnabled(bool enabled)
{
    std::vector<GeoRect> rects;
    std::vector<int> indexes;

    const std::lock_guard<std::mutex> lock(m_sourcesMutex);

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

bool TileFactory::chartEnabledForTile(const std::string &chart,
                                      const std::string &tileId) const
{
    std::unordered_map<std::string, TileSettings>::const_iterator it = m_tileSettings.find(tileId);

    if (it == m_tileSettings.end()) {
        return true;
    }
    TileSettings tileSettings = it->second;

    std::vector<std::string>::iterator it2 = find(tileSettings.disabledCharts.begin(),
                                                  tileSettings.disabledCharts.end(),
                                                  chart);
    return (it2 == tileSettings.disabledCharts.end());
}

std::vector<TileFactory::Source> TileFactory::sourceCandidates(const GeoRect &rect,
                                                               double pixelsPerLon)
{
    const std::lock_guard<std::mutex> lock(m_sourcesMutex);
    std::vector<TileFactory::Source> validSources;

    for (auto it = m_sources.cbegin(); it < m_sources.cend(); it++) {
        const TileFactory::Source &source = *it;
        assert(source.tileSource);

        if (!source.enabled) {
            continue;
        }

        int scaleActual = 52246 / (pixelsPerLon / 2560 * 0.6);

        const std::shared_ptr<ITileSource> &tileSource = source.tileSource;

        if (!tileSource->extent().intersects(rect)) {
            continue;
        }

        // Avoid showing too detailed maps when zoomed out, but always show last
        // chart with the lowest detail
        if (scaleActual / 4 > tileSource->scale() && std::next(it, 1) != m_sources.end()) {
            continue;
        }

        validSources.push_back(source);
    }

    return validSources;
}

std::vector<TileFactory::ChartInfo> TileFactory::chartInfo(const GeoRect &rect,
                                                           double pixelsPerLongitude)
{
    std::vector<TileFactory::ChartInfo> result;
    std::vector<Source> candidates = sourceCandidates(rect, pixelsPerLongitude);

    std::string tileId = FileHelper::tileId(rect, pixelsPerLongitude);

    for (const auto &source : candidates) {
        ChartInfo chartInfo;
        chartInfo.name = source.name;
        chartInfo.enabled = chartEnabledForTile(source.name, tileId);

        const std::shared_ptr<ITileSource> &tileSource = source.tileSource;
        result.push_back(chartInfo);
    }
    return result;
}

std::vector<std::shared_ptr<Chart>> TileFactory::tileData(const GeoRect &rect,
                                                          double pixelsPerLongitude)
{
    std::vector<std::shared_ptr<Chart>> chartDatas;
    auto sources = sourceCandidates(rect, pixelsPerLongitude);

    std::string tileId = FileHelper::tileId(rect, pixelsPerLongitude);
    CoverageRatio coverageRatio(rect);

    for (const auto &source : sources) {
        const std::shared_ptr<ITileSource> &tileSource = source.tileSource;
        std::shared_ptr<Chart> tileData = tileSource->create(rect, pixelsPerLongitude);

        if (!tileData) {
            std::cerr << "No tile data created" << std::endl;
            continue;
        }

        if (!chartEnabledForTile(source.name, tileId)) {
            continue;
        }

        chartDatas.push_back(tileData);
        coverageRatio.accumulate(tileData->coverage());

        if (coverageRatio.ratio() >= coverageAccpetanceThreshold) {
            break;
        }
    }

    return std::vector<std::shared_ptr<Chart>>(chartDatas.rbegin(), chartDatas.rend());
}

std::vector<TileFactory::Tile> TileFactory::tiles(const Pos &center,
                                                  float pixelsPerLongitude,
                                                  int width,
                                                  int height)
{
    assert(width > 0 && height > 0);

    double east = Mercator::mercatorWidthInverse(center.lon(), width / 2, pixelsPerLongitude);
    double south = Mercator::mercatorHeightInverse(center.lat(), height / 2, pixelsPerLongitude);
    double west = Mercator::mercatorWidthInverse(center.lon(), -width / 2, pixelsPerLongitude);
    double north = Mercator::mercatorHeightInverse(center.lat(), -height / 2, pixelsPerLongitude);

    const GeoRect viewport(north, south, west, east);
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

    const std::lock_guard<std::mutex> lock(m_sourcesMutex);

    for (const auto &tileRect : tileLocations) {
        for (const auto &source : m_sources) {
            if (tileRect.intersects(source.tileSource->extent())) {
                Tile tile { FileHelper::tileId(tileRect, maxPixelsPerLon),
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

    const auto mercatorTiles = mercatortile::tiles(convertToMercatorTileBox(rect), zoom);
    std::vector<GeoRect> geoRects(mercatorTiles.size());

    std::transform(mercatorTiles.cbegin(), mercatorTiles.cend(), geoRects.begin(),
                   [](const mercatortile::Tile &tile) {
                       return convertToGeoRect(mercatortile::bounds(tile));
                   });

    return geoRects;
}

bool TileFactory::hasSource(const std::string &name)
{
    const std::lock_guard<std::mutex> lock(m_sourcesMutex);
    for (const auto &source : m_sources) {
        if (source.name == name) {
            return true;
        }
    }
    return false;
}

void TileFactory::setSources(const std::vector<TileFactory::Source> &sources)
{
    const std::lock_guard<std::mutex> lock(m_sourcesMutex);
    std::vector<TileFactory::Source> qualifiedSources;
    std::vector<GeoRect> rois;

    for (const auto &source : m_sources) {
        rois.push_back(source.tileSource->extent());
    }

    for (const auto &source : sources) {
        if (!source.tileSource) {
            std::cerr << "Not adding null source" << std::endl;
            continue;
        }
        qualifiedSources.push_back(source);
        rois.push_back(source.tileSource->extent());
    }

    m_previousTileLocations.clear();
    m_sources = qualifiedSources;

    std::sort(m_sources.begin(),
              m_sources.end(),
              [](const TileFactory::Source &a, const TileFactory::Source &b) -> bool {
                  return a.tileSource->scale() < b.tileSource->scale();
              });

    if (m_updateCallback) {
        m_updateCallback();
    }

    if (m_chartsChangedCb) {
        m_chartsChangedCb(rois);
    }
}

void TileFactory::setTileSettings(const std::string &tileId, TileSettings tileSettings)
{
    std::unordered_map<std::string, TileSettings>::const_iterator it = m_tileSettings.find(tileId);
    if (it != m_tileSettings.end() && it->second.disabledCharts == tileSettings.disabledCharts) {
        return;
    }

    m_tileSettings[tileId] = tileSettings;

    if (!m_tileDataChangedCallback) {
        return;
    }

    m_tileDataChangedCallback({ tileId });
}

std::optional<GeoRect> TileFactory::totalExtent()
{
    if (m_sources.empty()) {
        return {};
    }

    const std::lock_guard<std::mutex> lock(m_sourcesMutex);

    std::vector<double> lefts(m_sources.size());
    std::vector<double> rights(m_sources.size());
    std::vector<double> tops(m_sources.size());
    std::vector<double> bottoms(m_sources.size());

    std::transform(m_sources.begin(), m_sources.end(), lefts.begin(), [](const Source &source) {
        return source.tileSource->extent().left();
    });

    std::transform(m_sources.begin(), m_sources.end(), rights.begin(), [](const Source &source) {
        return source.tileSource->extent().right();
    });

    std::transform(m_sources.begin(), m_sources.end(), tops.begin(), [](const Source &source) {
        return source.tileSource->extent().top();
    });

    std::transform(m_sources.begin(), m_sources.end(), bottoms.begin(), [](const Source &source) {
        return source.tileSource->extent().bottom();
    });

    GeoRect extent(*std::max_element(tops.begin(), tops.end()),
                   *std::min_element(bottoms.begin(), bottoms.end()),
                   *std::min_element(lefts.begin(), lefts.end()),
                   *std::max_element(rights.begin(), rights.end()));
    if (extent.isNull()) {
        return {};
    }

    return extent;
}
