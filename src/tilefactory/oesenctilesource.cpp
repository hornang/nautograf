#include <chrono>
#include <filesystem>
#include <random>
#include <sstream>
#include <thread>

#include "tilefactory/chartclipper.h"
#include "tilefactory/mercator.h"
#include "tilefactory/oesenctilesource.h"

static constexpr int clippingMarginInPixels = 50;

OesencTileSource::OesencTileSource(const std::string &file,
                                   const std::string &name,
                                   const std::string &tileDir)
    : m_name(name)
    , m_oesencChart(file)
{
    m_valid = m_oesencChart.readHeaders();
    m_extent = m_oesencChart.extent();
    m_scale = m_oesencChart.nativeScale();

    ensureTileDir(tileDir);
}

OesencTileSource::OesencTileSource(const std::vector<std::byte> &data,
                                   const std::string &name,
                                   const std::string &tileDir)
    : m_name(name)
    , m_oesencChart(data)
{
    m_valid = m_oesencChart.readHeaders();
    m_extent = m_oesencChart.extent();
    m_scale = m_oesencChart.nativeScale();

    ensureTileDir(tileDir);
}

void OesencTileSource::ensureTileDir(const std::string &tileDir)
{
    std::stringstream ss;

    // When schema has incompatible changes then another directory should be used
    ss << std::hex << Chart::typeId();
    const std::string s = ss.str();
    std::filesystem::path dir;
    dir.append(tileDir);
    dir.append(ss.str());
    m_tileDir = dir.string();
    std::filesystem::create_directories(dir);
}

bool OesencTileSource::isValid() const
{
    return m_valid;
}

OesencTileSource::~OesencTileSource()
{
}

GeoRect OesencTileSource::extent() const
{
    return GeoRect(m_extent.top(), m_extent.bottom(), m_extent.left(), m_extent.right());
}

std::shared_ptr<Chart> OesencTileSource::create(const GeoRect &boundingBox,
                                                int pixelsPerLongitude)
{
    const std::string id = tileId(boundingBox, pixelsPerLongitude);

    std::shared_ptr<std::mutex> tileMutex;
    {
        std::lock_guard guard(m_tileMutexesMutex);
        auto it = m_tileMutexes.find(id);
        if (it == m_tileMutexes.end()) {
            m_tileMutexes[id] = std::make_shared<std::mutex>();
        }
        tileMutex = m_tileMutexes[id];
    }

    std::lock_guard tileGuard(*tileMutex.get());
    std::filesystem::path path = m_tileDir;
    path.append(m_name);
    path.append(id + std::string(".bin"));
    std::string tilefile = path.string();

    if (std::filesystem::exists(tilefile)) {
        auto tile = std::make_shared<Chart>(tilefile);
        std::lock_guard guard(m_tileMutexesMutex);
        m_tileMutexes.erase(id);
        return tile;
    }

    if (!std::filesystem::exists(path.parent_path())) {
        std::filesystem::create_directory(path.parent_path());
    }
    auto tile = generateTile(boundingBox, pixelsPerLongitude);
    if (tile) {
        tile->write(tilefile);
    }

    std::lock_guard guard(m_tileMutexesMutex);
    m_tileMutexes.erase(id);
    return tile;
}

std::string OesencTileSource::tileId(const GeoRect &boundingBox, int pixelsPerLongitude)
{
    std::stringstream ss;
    ss << boundingBox.top();
    ss << boundingBox.left();
    ss << boundingBox.bottom();
    ss << boundingBox.right();
    ss << pixelsPerLongitude;
    unsigned int h1 = std::hash<std::string> {}(ss.str());

    static const std::string_view hex_chars = "0123456789abcdef";

    std::mt19937 mt(h1);

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

std::shared_ptr<Chart> OesencTileSource::generateTile(const GeoRect &boundingBox,
                                                      int pixelsPerLongitude)
{
    double longitudeMargin = Mercator::mercatorWidthInverse(boundingBox.left(),
                                                            clippingMarginInPixels,
                                                            pixelsPerLongitude)
        - boundingBox.left();
    double latitudeMargin = boundingBox.top()
        - Mercator::mercatorHeightInverse(boundingBox.top(),
                                          clippingMarginInPixels,
                                          pixelsPerLongitude);

    double latitudeResolution = boundingBox.top()
        - Mercator::mercatorHeightInverse(boundingBox.top(),
                                          2,
                                          pixelsPerLongitude);
    double longitudeResolution = Mercator::mercatorWidthInverse(boundingBox.left(),
                                                                2,
                                                                pixelsPerLongitude)
        - boundingBox.left();

    m_chartMutex.lock();
    if (!m_entireChart) {
        if (m_oesencChart.read()) {

            const oesenc::Rect rect = m_oesencChart.extent();
            const GeoRect extent(rect.top(), rect.bottom(), rect.left(), rect.right());
            m_entireChart = std::make_unique<Chart>(m_oesencChart.s57(),
                                                    extent,
                                                    m_name,
                                                    m_oesencChart.nativeScale());
        } else {
            std::cerr << "Failed read chart: " << m_name << std::endl;
            m_chartMutex.unlock();
            return {};
        }
    }
    m_chartMutex.unlock();
    ChartClipper::Config clipConfig;
    clipConfig.box = boundingBox;
    clipConfig.latitudeMargin = latitudeMargin;
    clipConfig.longitudeMargin = longitudeMargin;
    clipConfig.longitudeResolution = longitudeResolution;
    clipConfig.latitudeResolution = latitudeResolution;
    clipConfig.maxPixelsPerLongitude = pixelsPerLongitude;
    return std::make_shared<Chart>(m_entireChart->clipped(clipConfig, &m_soundingCache));
}
