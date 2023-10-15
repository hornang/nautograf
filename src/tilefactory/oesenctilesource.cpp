#include <chrono>
#include <filesystem>
#include <random>
#include <sstream>
#include <thread>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/adapted/std_array.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/register/linestring.hpp>
#include <boost/geometry/geometries/register/point.hpp>

#include "filehelper.h"
#include "tilefactory/chartclipper.h"
#include "tilefactory/mercator.h"
#include "tilefactory/oesenctilesource.h"

using namespace std;

namespace {
    constexpr int clippingMarginInPixels = 6;
}

OesencTileSource::OesencTileSource(const string &file,
                                   const string &name,
                                   const string &baseTileDir)
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

OesencTileSource::OesencTileSource(const vector<byte> &data,
                                   const string &name,
                                   const string &baseTileDir)
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

BOOST_GEOMETRY_REGISTER_POINT_2D_GET_SET(oesenc::Position,
                                         double,
                                         cs::cartesian,
                                         latitude,
                                         longitude,
                                         setLatitude,
                                         setLongitude)

BOOST_GEOMETRY_REGISTER_LINESTRING(vector<oesenc::Position>)

bool OesencTileSource::convertChartToInternalFormat(float lineEpsilon, int pixelsPerLon)
{
    if (m_oesencSourceType == OesencSourceType::Invalid) {
        return false;
    }

    string decimatedFileName = FileHelper::internalChartFileName(m_tileDir,
                                                                 m_name,
                                                                 pixelsPerLon);

    const lock_guard<mutex> lock(m_internalChartMutex);

    if (filesystem::exists(decimatedFileName)) {
        return true;
    }

    unique_ptr<capnp::MallocMessageBuilder> capnpMessage;
    unique_ptr<oesenc::ChartFile> oesencChart;

    using Line = vector<oesenc::Position>;

    oesenc::ChartFile::Config oesencConfig;
    oesencConfig.vectorEdgeDecimator = [=](const Line &line) -> Line {
        Line simplifiedLine;
        boost::geometry::simplify(line, simplifiedLine, lineEpsilon);
        return simplifiedLine;
    };

    switch (m_oesencSourceType) {
    case OesencSourceType::File:
        oesencChart = make_unique<oesenc::ChartFile>(m_oesencFile, oesencConfig);
        break;
    case OesencSourceType::Vector:
        oesencChart = make_unique<oesenc::ChartFile>(m_oesencData, oesencConfig);
        break;
    }

    if (!oesencChart->read()) {
        return false;
    }

    readOesencMetaData(oesencChart.get());
    capnpMessage = Chart::buildFromS57(oesencChart->s57(), m_extent, m_name, m_scale);

    filesystem::path targetPath = decimatedFileName;

    if (!filesystem::exists(targetPath.parent_path())) {
        error_code errorCode;
        if (!filesystem::create_directory(targetPath.parent_path(), errorCode)) {
            cerr << "Failed to create dir " << targetPath.parent_path() << endl;
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

shared_ptr<Chart> OesencTileSource::create(const GeoRect &boundingBox,
                                           int pixelsPerLongitude)
{
    const string id = FileHelper::tileId(boundingBox, pixelsPerLongitude);

    shared_ptr<mutex> tileMutex;
    {
        lock_guard guard(m_tileMutexesMutex);
        auto it = m_tileMutexes.find(id);
        if (it == m_tileMutexes.end()) {
            m_tileMutexes[id] = make_shared<mutex>();
        }
        tileMutex = m_tileMutexes[id];
    }

    lock_guard tileGuard(*tileMutex.get());
    string tilefile = FileHelper::tileFileName(m_tileDir, m_name, id);

    if (filesystem::exists(tilefile)) {
        shared_ptr<Chart> tile = Chart::open(tilefile);

        if (!tile) {
            cerr << "Failed to create chart from: " << tilefile << endl;
            return {};
        }

        lock_guard guard(m_tileMutexesMutex);
        m_tileMutexes.erase(id);
        return tile;
    }

    auto tile = generateTile(boundingBox, pixelsPerLongitude);
    lock_guard guard(m_tileMutexesMutex);
    m_tileMutexes.erase(id);
    return tile;
}

shared_ptr<Chart> OesencTileSource::generateTile(const GeoRect &boundingBox,
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

    string internalChartFileName = FileHelper::internalChartFileName(m_tileDir,
                                                                     m_name,
                                                                     pixelsPerLongitude);

    if (!filesystem::exists(internalChartFileName)) {
        float epsilon = 2 * min(longitudeResolution, latitudeResolution);
        if (!convertChartToInternalFormat(epsilon, pixelsPerLongitude)) {
            cerr << "Failed to convert chart to internal format" << endl;
            return {};
        }
    }

    shared_ptr<Chart> entireChart = Chart::open(internalChartFileName);

    if (!entireChart) {
        cerr << "Failed to open " << internalChartFileName << endl;
        return {};
    }

    assert(entireChart->nativeScale() != 0);

    string id = FileHelper::tileId(boundingBox, pixelsPerLongitude);
    string tileFile = FileHelper::tileFileName(m_tileDir, m_name, id);

    ChartClipper::Config clipConfig;
    clipConfig.box = boundingBox;
    clipConfig.latitudeMargin = latitudeMargin;
    clipConfig.longitudeMargin = longitudeMargin;
    clipConfig.longitudeResolution = longitudeResolution;
    clipConfig.latitudeResolution = latitudeResolution;
    clipConfig.maxPixelsPerLongitude = pixelsPerLongitude;

    unique_ptr<capnp::MallocMessageBuilder> clippedChart = entireChart->buildClipped(clipConfig);

    assert(clippedChart);

    if (!Chart::write(clippedChart.get(), tileFile)) {
        cerr << "Failed to write " << tileFile << endl;
        return {};
    }

    return Chart::open(tileFile);
}
