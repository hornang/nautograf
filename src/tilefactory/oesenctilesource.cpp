#include <chrono>
#include <filesystem>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

#include <slippy_map_tiles/lib.rs.h>

#include "filehelper.h"
#include "oesenc/serverreader.h"
#include "tilefactory/catalog.h"
#include "tilefactory/chartclipper.h"
#include "tilefactory/mercator.h"
#include "tilefactory/oesenctilesource.h"

using namespace std;

namespace {
constexpr int clippingMarginInPixels = 6;
mutex catalogueMutex;
}

OesencTileSource::OesencTileSource(Catalog *catalogue, string_view name,
                                   string_view baseTileDir)
    : m_name(name)
    , m_tileDir(FileHelper::getTileDir(string(baseTileDir), Chart::typeId()))
    , m_catalogue(catalogue)
{
    lock_guard guard(catalogueMutex);
    auto stream = m_catalogue->openChart(name);
    oesenc::ChartFile chart = oesenc::ChartFile(*stream);

    if (chart.readHeaders()) {
        readOesencMetaData(&chart);
        m_valid = true;
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

bool OesencTileSource::convertChartToInternalFormat(float lineEpsilon, int pixelsPerLon)
{
    std::string decimatedFileName = FileHelper::internalChartFileName(m_tileDir,
                                                                      m_name,
                                                                      pixelsPerLon);

    const lock_guard<mutex> lock(m_internalChartMutex);

    if (filesystem::exists(decimatedFileName)) {
        return true;
    }

    std::unique_ptr<capnp::MallocMessageBuilder> capnpMessage;

    using Line = vector<oesenc::Position>;

    oesenc::ChartFile::Config oesencConfig;
    oesencConfig.vectorEdgeDecimator = [=](const Line &line) -> Line {
        if (line.size() < 2) {
            return line;
        }

        ::rust::Vec<slippy_map_tiles::Pos> input;
        input.reserve(line.size());
        for (const oesenc::Position &pos : line) {
            input.push_back({ pos.latitude(), pos.longitude() });
        }

        ::rust::Vec<slippy_map_tiles::Pos> simplified = slippy_map_tiles::simplify(input, lineEpsilon);

        for (const oesenc::Position &pos : line) {
            input.push_back({ pos.latitude(), pos.longitude() });
        }

        Line simplifiedLine;
        simplifiedLine.reserve(simplified.size());

        for (const slippy_map_tiles::Pos &pos : simplified) {
            simplifiedLine.push_back({ pos.x, pos.y });
        }

        return simplifiedLine;
    };

    lock_guard guard(catalogueMutex);
    shared_ptr<istream> stream = m_catalogue->openChart(m_name);
    unique_ptr<oesenc::ChartFile> oesencChart = make_unique<oesenc::ChartFile>(*stream, oesencConfig);

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
    return m_valid;
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
