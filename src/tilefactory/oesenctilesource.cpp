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

{
    oesenc::ChartFile chart = oesenc::ChartFile(file);

    if (chart.readHeaders()) {
        readOesencMetaData(&chart);
        m_oesencFile = file;
        m_oesencSourceType = OesencSourceType::File;
        ensureTileDir(tileDir);
    }
}

OesencTileSource::OesencTileSource(const std::vector<std::byte> &data,
                                   const std::string &name,
                                   const std::string &tileDir)
    : m_name(name)
{
    oesenc::ChartFile chart = oesenc::ChartFile(data);

    if (chart.readHeaders()) {
        readOesencMetaData(&chart);
        m_oesencData = data;
        m_oesencSourceType = OesencSourceType::Vector;
        ensureTileDir(tileDir);
    }
}

void OesencTileSource::readOesencMetaData(const oesenc::ChartFile *chart)
{
    assert(chart);
    m_scale = chart->nativeScale();
    m_extent = fromOesencRect(chart->extent());
}

GeoRect OesencTileSource::fromOesencRect(const oesenc::Rect &src)
{
    return GeoRect(src.top(), src.bottom(), src.left(), src.right());
}

bool OesencTileSource::convertChartToInternalFormat()
{
    if (m_oesencSourceType == OesencSourceType::Invalid) {
        return false;
    }

    std::string filename = internalChartFileName();

    const std::lock_guard<std::mutex> lock(m_internalChartMutex);

    if (std::filesystem::exists(filename)) {
        return true;
    }

    std::unique_ptr<capnp::MallocMessageBuilder> capnpMessage;
    std::unique_ptr<oesenc::ChartFile> oesencChart;

    switch (m_oesencSourceType) {
    case OesencSourceType::File:
        oesencChart = std::make_unique<oesenc::ChartFile>(m_oesencFile);
        break;
    case OesencSourceType::Vector:
        oesencChart = std::make_unique<oesenc::ChartFile>(m_oesencData);
        break;
    }

    if (!oesencChart->read()) {
        return false;
    }

    readOesencMetaData(oesencChart.get());
    capnpMessage = Chart::buildFromS57(oesencChart->s57(), m_extent, m_name, m_scale);

    std::filesystem::path targetPath(filename);

    if (!std::filesystem::exists(targetPath.parent_path())) {
        std::filesystem::create_directory(targetPath.parent_path());
    }

    if (!Chart::write(capnpMessage.get(), filename)) {
        return false;
    }

    return true;
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
    return m_oesencSourceType != OesencSourceType::Invalid;
}

OesencTileSource::~OesencTileSource()
{
}

GeoRect OesencTileSource::extent() const
{
    return m_extent;
}

std::string OesencTileSource::tileFileName(const std::string &id)
{
    std::filesystem::path path(m_tileDir);
    return (path / m_name / (id + std::string(".bin"))).string();
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
    std::string tilefile = tileFileName(id);

    if (std::filesystem::exists(tilefile)) {
        std::shared_ptr<Chart> tile = Chart::open(tilefile);

        if (!tile) {
            std::cerr << "Failed to create chart from: " << tilefile << std::endl;
            return {};
        }

        std::lock_guard guard(m_tileMutexesMutex);
        m_tileMutexes.erase(id);
        return tile;
    }

    auto tile = generateTile(boundingBox, pixelsPerLongitude);
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

    if (!std::filesystem::exists(internalChartFileName())) {
        if (!convertChartToInternalFormat()) {
            std::cerr << "Failed to convert chart to internal format" << std::endl;
            return {};
        }
    }

    std::shared_ptr<Chart> entireChart = Chart::open(internalChartFileName());

    if (!entireChart) {
        std::cerr << "Failed to open " << internalChartFileName() << std::endl;
        return {};
    }

    std::string id = tileId(boundingBox, pixelsPerLongitude);
    std::string tileFile = tileFileName(id);

    ChartClipper::Config clipConfig;
    clipConfig.box = boundingBox;
    clipConfig.latitudeMargin = latitudeMargin;
    clipConfig.longitudeMargin = longitudeMargin;
    clipConfig.longitudeResolution = longitudeResolution;
    clipConfig.latitudeResolution = latitudeResolution;
    clipConfig.maxPixelsPerLongitude = pixelsPerLongitude;

    std::unique_ptr<capnp::MallocMessageBuilder> clippedChart = entireChart->buildClipped(clipConfig);

    if (!Chart::write(clippedChart.get(), tileFileName(id))) {
        std::cerr << "Failed to write " << tileFileName(id) << std::endl;
        return {};
    }

    return Chart::open(tileFile);
}

std::string OesencTileSource::internalChartFileName() const
{
    std::filesystem::path path(m_tileDir);
    return (path / m_name / "all.bin").string();
}
