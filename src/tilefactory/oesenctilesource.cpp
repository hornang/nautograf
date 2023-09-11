#include <chrono>
#include <filesystem>
#include <random>
#include <sstream>
#include <thread>

#include "filehelper.h"
#include "tilefactory/chartclipper.h"
#include "tilefactory/mercator.h"
#include "tilefactory/oesenctilesource.h"

namespace {
    constexpr int clippingMarginInPixels = 6;
}

OesencTileSource::OesencTileSource(const std::string &file,
                                   const std::string &name,
                                   const std::string &baseTileDir)
    : m_name(name)
    , m_tileDir(FileHelper::getTileDir(baseTileDir, Chart::typeId()))
{
    oesenc::ChartFile chart = oesenc::ChartFile(file);

    if (chart.readHeaders()) {
        readOesencMetaData(&chart);
        m_oesencFile = file;
        m_oesencSourceType = OesencSourceType::File;
    }
}

OesencTileSource::OesencTileSource(const std::vector<std::byte> &data,
                                   const std::string &name,
                                   const std::string &baseTileDir)
    : m_name(name)
    , m_tileDir(FileHelper::getTileDir(baseTileDir, Chart::typeId()))
{
    oesenc::ChartFile chart = oesenc::ChartFile(data);

    if (chart.readHeaders()) {
        readOesencMetaData(&chart);
        m_oesencData = data;
        m_oesencSourceType = OesencSourceType::Vector;
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

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/adapted/std_array.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/register/linestring.hpp>
#include <boost/geometry/geometries/register/point.hpp>

BOOST_GEOMETRY_REGISTER_POINT_2D_GET_SET(oesenc::Position,
                                         double,
                                         cs::cartesian,
                                         latitude,
                                         longitude,
                                         setLatitude,
                                         setLongitude)

BOOST_GEOMETRY_REGISTER_LINESTRING(std::vector<oesenc::Position>)

bool OesencTileSource::convertChartToInternalFormat(float lineEpsilon, int pixelsPerLon)
{
    if (m_oesencSourceType == OesencSourceType::Invalid) {
        return false;
    }

    std::string decimatedFileName = FileHelper::internalChartFileName(m_tileDir,
                                                                      m_name,
                                                                      pixelsPerLon);

    const std::lock_guard<std::mutex> lock(m_internalChartMutex);

    if (std::filesystem::exists(decimatedFileName)) {
        return true;
    }

    std::unique_ptr<capnp::MallocMessageBuilder> capnpMessage;
    std::unique_ptr<oesenc::ChartFile> oesencChart;

    using Line = std::vector<oesenc::Position>;

    oesenc::ChartFile::Config oesencConfig;
    oesencConfig.vectorEdgeDecimator = [=](const Line &line) -> Line {
        Line simplifiedLine;
        boost::geometry::simplify(line, simplifiedLine, lineEpsilon);
        return simplifiedLine;
    };

    switch (m_oesencSourceType) {
    case OesencSourceType::File:
        oesencChart = std::make_unique<oesenc::ChartFile>(m_oesencFile, oesencConfig);
        break;
    case OesencSourceType::Vector:
        oesencChart = std::make_unique<oesenc::ChartFile>(m_oesencData, oesencConfig);
        break;
    }

    if (!oesencChart->read()) {
        return false;
    }

    readOesencMetaData(oesencChart.get());
    capnpMessage = Chart::buildFromS57(oesencChart->s57(), m_extent, m_name, m_scale);

    std::filesystem::path targetPath = decimatedFileName;

    if (!std::filesystem::exists(targetPath.parent_path())) {
        std::error_code errorCode;
        if (!std::filesystem::create_directory(targetPath.parent_path(), errorCode)) {
            std::cerr << "Failed to create dir " << targetPath.parent_path() << std::endl;
        }
    }

    if (!Chart::write(capnpMessage.get(), decimatedFileName)) {
        return false;
    }

    return true;
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

std::shared_ptr<Chart> OesencTileSource::create(const GeoRect &boundingBox,
                                                int pixelsPerLongitude)
{
    const std::string id = FileHelper::tileId(boundingBox, pixelsPerLongitude);

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
    std::string tilefile = FileHelper::tileFileName(m_tileDir, m_name, id);

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

    std::string internalChartFileName = FileHelper::internalChartFileName(m_tileDir,
                                                                          m_name,
                                                                          pixelsPerLongitude);

    if (!std::filesystem::exists(internalChartFileName)) {
        float epsilon = 2 * std::min(longitudeResolution, latitudeResolution);
        if (!convertChartToInternalFormat(epsilon, pixelsPerLongitude)) {
            std::cerr << "Failed to convert chart to internal format" << std::endl;
            return {};
        }
    }

    std::shared_ptr<Chart> entireChart = Chart::open(internalChartFileName);

    if (!entireChart) {
        std::cerr << "Failed to open " << internalChartFileName << std::endl;
        return {};
    }

    assert(entireChart->nativeScale() != 0);

    std::string id = FileHelper::tileId(boundingBox, pixelsPerLongitude);
    std::string tileFile = FileHelper::tileFileName(m_tileDir, m_name, id);

    ChartClipper::Config clipConfig;
    clipConfig.box = boundingBox;
    clipConfig.latitudeMargin = latitudeMargin;
    clipConfig.longitudeMargin = longitudeMargin;
    clipConfig.longitudeResolution = longitudeResolution;
    clipConfig.latitudeResolution = latitudeResolution;
    clipConfig.maxPixelsPerLongitude = pixelsPerLongitude;

    std::unique_ptr<capnp::MallocMessageBuilder> clippedChart = entireChart->buildClipped(clipConfig);

    assert(clippedChart);

    if (!Chart::write(clippedChart.get(), tileFile)) {
        std::cerr << "Failed to write " << tileFile << std::endl;
        return {};
    }

    return Chart::open(tileFile);
}
