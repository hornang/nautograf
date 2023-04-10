#include <chrono>
#include <thread>

#include "cutlines/cutlines.h"
#include "tilefactory/chart.h"
#include "tilefactory/georect.h"
#include "tilefactory/mercator.h"
#include "tilefactory/triangulator.h"

namespace {

template <typename T>
void copyLinesToBuilder(typename T::Builder dst, const std::vector<cutlines::Line> &lines)
{
    capnp::List<ChartData::Line>::Builder dstLines = dst.initLines(static_cast<unsigned int>(lines.size()));

    int i = 0;
    for (const cutlines::Line &line : lines) {
        ChartData::Line::Builder lineBuilder = dstLines[i++];
        capnp::List<ChartData::Position>::Builder positions = lineBuilder.initPositions(static_cast<unsigned int>(line.size()));
        int i = 0;
        for (const cutlines::Point &pos : line) {
            ChartData::Position::Builder dstPos = positions[i++];
            dstPos.setLatitude(pos[1]);
            dstPos.setLongitude(pos[0]);
        }
    }
}

template <typename T>
struct ClippedItem
{
    std::vector<ChartClipper::Polygon> polygons;
    std::vector<cutlines::Line> lines;
    typename T::Reader sourceItem;
};

using Polygon = std::vector<Pos>;

void toCapnPolygon(capnp::List<ChartData::Position>::Builder dst, const Polygon &src)
{
    int positionIndex = 0;
    for (const Pos &pos : src) {
        auto dstPos = dst[positionIndex++];
        dstPos.setLatitude(pos.lat());
        dstPos.setLongitude(pos.lon());
    }
}

void toCapnPolygons(::capnp::List<capnp::List<ChartData::Position>>::Builder dst,
                    const std::vector<Polygon> &src)
{
    int polygonIndex = 0;

    for (const Polygon &srcPolygon : src) {
        dst.init(polygonIndex, (unsigned int)srcPolygon.size());
        auto dstPositions = dst[polygonIndex];

        int positionIndex = 0;
        for (const Pos &pos : srcPolygon) {
            auto dstPos = dstPositions[positionIndex];
            dstPos.setLatitude(pos.lat());
            dstPos.setLongitude(pos.lon());
            positionIndex++;
        }

        polygonIndex++;
    }
}

template <typename T>
void copyPolygonsToBuilder(typename T::Builder dst, const std::vector<ChartClipper::Polygon> &src)
{
    capnp::List<ChartData::Polygon>::Builder dstPolygons = dst.initPolygons((unsigned int)src.size());

    int polygonIndex = 0;
    for (const ChartClipper::Polygon &polygon : src) {
        ChartData::Polygon::Builder dstPolygon = dstPolygons[polygonIndex++];
        auto main = dstPolygon.initMain(static_cast<unsigned int>(polygon.main.size()));
        toCapnPolygon(main, polygon.main);
        auto holes = dstPolygon.initHoles(static_cast<unsigned int>(polygon.holes.size()));
        toCapnPolygons(holes, polygon.holes);
    }
}

std::vector<ChartClipper::Polygon> clipPolygons(const capnp::List<ChartData::Polygon>::Reader &polygons,
                                                const ChartClipper::Config &config)
{
    std::vector<ChartClipper::Polygon> clipped;

    for (const ChartData::Polygon::Reader &polygon : polygons) {
        std::vector<ChartClipper::Polygon> polygons = ChartClipper::clipPolygon(polygon, config);
        clipped.insert(clipped.end(), polygons.begin(), polygons.end());
    }

    return clipped;
}

template <typename T>
void clipPolygonItems(const typename capnp::List<T>::Reader &src,
                      ChartClipper::Config config,
                      std::function<typename capnp::List<T>::Builder(unsigned int length)> init,
                      std::function<void(typename T::Builder &, const typename T::Reader &)> copyFunction)
{
    std::vector<ClippedItem<T>> clippedItems;

    for (const typename T::Reader &element : src) {
        std::vector<ChartClipper::Polygon> polygons = clipPolygons(element.getPolygons(), config);

        if (!polygons.empty()) {
            clippedItems.push_back({ polygons, {}, element });
        }
    }

    typename capnp::List<T>::Builder list = init(static_cast<unsigned int>(clippedItems.size()));

    int i = 0;

    for (const ClippedItem<T> &item : clippedItems) {
        typename T::Builder builder = list[i++];
        if (copyFunction) {
            copyFunction(builder, item.sourceItem);
        }
        copyPolygonsToBuilder<T>(builder, item.polygons);
    }
}

std::vector<cutlines::Line> clipLines(const capnp::List<ChartData::Line>::Reader &lines,
                                      const cutlines::Rect &clipRect)
{
    std::vector<cutlines::Line> clipped;

    for (const ChartData::Line::Reader &line : lines) {
        cutlines::Line dstLine;

        for (const ChartData::Position::Reader &pos : line.getPositions()) {
            dstLine.push_back({ pos.getLongitude(), pos.getLatitude() });
        }

        std::vector<cutlines::Line> clippedLines = cutlines::clip(dstLine, clipRect);
        clipped.insert(clipped.end(), clippedLines.begin(), clippedLines.end());
    }

    return clipped;
}

cutlines::Rect toCutlinesRect(const GeoRect &rect, ChartClipper::Config &config)
{
    return cutlines::Rect { rect.left() - config.longitudeMargin,
                            rect.right() + config.longitudeMargin,
                            rect.bottom() - config.latitudeMargin,
                            rect.top() + config.latitudeMargin };
}

template <typename T>
void clipLineItems(const typename capnp::List<T>::Reader &src,
                   ChartClipper::Config config,
                   std::function<typename capnp::List<T>::Builder(unsigned int length)> init,
                   std::function<void(typename T::Builder &, const typename T::Reader &)> copyFunction)
{
    std::vector<ClippedItem<T>> clippedItems;

    for (const typename T::Reader &element : src) {
        std::vector<cutlines::Line> lines = clipLines(element.getLines(),
                                                      toCutlinesRect(config.box, config));

        if (!lines.empty()) {
            clippedItems.push_back({ {}, lines, element });
        }
    }

    typename capnp::List<T>::Builder list = init(static_cast<unsigned int>(clippedItems.size()));

    int i = 0;

    for (const ClippedItem<T> &item : clippedItems) {
        typename T::Builder builder = list[i++];
        if (copyFunction) {
            copyFunction(builder, item.sourceItem);
        }
        copyLinesToBuilder<T>(builder, item.lines);
    }
}

template <typename T>
void clipPolygonOrLineItems(const typename capnp::List<T>::Reader &src,
                            ChartClipper::Config config,
                            std::function<typename capnp::List<T>::Builder(unsigned int length)> init,
                            std::function<void(typename T::Builder &, const typename T::Reader &)> copyFunction)
{
    std::vector<ClippedItem<T>> clippedItems;

    for (const typename T::Reader &element : src) {
        std::vector<ChartClipper::Polygon> polygons = clipPolygons(element.getPolygons(), config);
        std::vector<cutlines::Line> lines = clipLines(element.getLines(),
                                                      toCutlinesRect(config.box, config));

        if (!polygons.empty() || !lines.empty()) {
            clippedItems.push_back({ polygons, lines, element });
        }
    }

    typename capnp::List<T>::Builder list = init(static_cast<unsigned int>(clippedItems.size()));

    int i = 0;

    for (const ClippedItem<T> &item : clippedItems) {
        typename T::Builder builder = list[i++];
        if (copyFunction) {
            copyFunction(builder, item.sourceItem);
        }
        copyPolygonsToBuilder<T>(builder, item.polygons);
        copyLinesToBuilder<T>(builder, item.lines);
    }
}

template <typename T>
struct ClippedPointItem
{
    Pos pos;
    typename T::Reader item;
};

template <typename T>
void clipPointItems(const typename capnp::List<T>::Reader &src,
                    ChartClipper::Config config,
                    std::function<typename capnp::List<T>::Builder(unsigned int length)> init,
                    std::function<void(typename T::Builder &, const typename T::Reader &)> copyFunction)
{
    std::vector<ClippedPointItem<T>> clipped;

    for (const auto &element : src) {
        const Pos pos(element.getPosition().getLatitude(), element.getPosition().getLongitude());
        if (config.box.contains(pos.lat(), pos.lon())) {
            clipped.push_back(ClippedPointItem<T> { pos, element });
        }
    }

    auto dst = init(static_cast<unsigned int>(clipped.size()));

    int i = 0;
    for (const auto &item : clipped) {
        auto element = dst[i++];
        element.getPosition().setLatitude(item.pos.lat());
        element.getPosition().setLongitude(item.pos.lon());
        copyFunction(element, item.item);
    }
}

template <typename T>
void computeCentroidFromPolygons(typename T::Builder builder)
{
    if (builder.getPolygons().size() == 0) {
        return;
    }

    double avgLat = 0;
    double avgLon = 0;
    int nPositions = 0;

    for (ChartData::Polygon::Builder polygon : builder.getPolygons()) {
        for (ChartData::Position::Builder pos : polygon.getMain()) {
            avgLat += pos.getLatitude();
            avgLon += pos.getLongitude();
            nPositions++;
        }
    }

    ChartData::Position::Builder centroid = builder.getCentroid();
    centroid.setLatitude(avgLat / nPositions);
    centroid.setLongitude(avgLon / nPositions);
}

void fromOesencPosToCapnp(capnp::List<ChartData::Position>::Builder &dst,
                          const std::vector<oesenc::Position> &src)
{
    int i = 0;
    for (const oesenc::Position &pos : src) {
        auto dstPos = dst[i++];
        dstPos.setLatitude(pos.latitude());
        dstPos.setLongitude(pos.longitude());
    }
}

template <typename T>
void loadPolygonsFromS57(typename T::Builder dst, const oesenc::S57 *s57)
{
    if (s57->polygons().empty()) {
        return;
    }

    capnp::List<ChartData::Polygon>::Builder polygons = dst.initPolygons(1);
    int polygonIndex = 0;

    capnp::List<capnp::List<ChartData::Position>>::Builder holes;
    if (s57->polygons().size() > 1) {
        holes = polygons[0].initHoles(static_cast<unsigned int>(s57->polygons().size() - 1));
    }

    for (const std::vector<oesenc::Position> &srcPolygon : s57->polygons()) {
        if (polygonIndex == 0) {
            capnp::List<ChartData::Position>::Builder main = polygons[0].initMain(static_cast<unsigned int>(srcPolygon.size()));
            fromOesencPosToCapnp(main, srcPolygon);
        } else {
            auto hole = holes.init(polygonIndex - 1,
                                   static_cast<unsigned int>(srcPolygon.size()));
            fromOesencPosToCapnp(hole, srcPolygon);
        }

        polygonIndex++;
    }
}

template <typename T>
void loadLinesFromS57(typename T::Builder dst, const oesenc::S57 *s57)
{
    const std::vector<oesenc::S57::MultiGeometry> &srcLines = s57->lines();
    capnp::List<ChartData::Line>::Builder dstLines = dst.initLines(static_cast<unsigned int>(srcLines.size()));

    int i = 0;
    for (const oesenc::S57::MultiGeometry &line : srcLines) {
        ChartData::Line::Builder dstLine = dstLines[i++];
        capnp::List<ChartData::Position>::Builder dstPositions = dstLine.initPositions(static_cast<unsigned int>(line.size()));

        int j = 0;
        for (const oesenc::Position &position : line) {
            ChartData::Position::Builder dstPos = dstPositions[j++];
            dstPos.setLatitude(position.latitude());
            dstPos.setLongitude(position.longitude());
        }
    }
}

using S57Vector = const std::vector<const oesenc::S57 *>;

void loadLandAreas(ChartData::Builder &root, const S57Vector &src)
{
    auto landAreas = root.initLandAreas(static_cast<unsigned int>(src.size()));

    unsigned int i = 0;
    for (const oesenc::S57 *s57 : src) {
        auto dstElement = landAreas[i++];
        auto name = s57->attribute<std::string>(oesenc::S57::Attribute::ObjectName);
        if (name.has_value()) {
            dstElement.setName(name.value());
        }

        loadPolygonsFromS57<ChartData::LandArea>(dstElement, s57);
        computeCentroidFromPolygons<ChartData::LandArea>(dstElement);
    }
}

bool isPolygonObject(const oesenc::S57 *obj)
{
    return !obj->polygons().empty();
}

bool isPointObject(const oesenc::S57 *obj)
{
    return obj->pointGeometry().has_value();
}

int countPolygonObjects(S57Vector &s57s)
{
    int n = 0;
    for (const auto &obj : s57s) {
        if (isPolygonObject(obj)) {
            n++;
        }
    }
    return n;
}

int countPointObjects(S57Vector &s57s)
{
    int n = 0;
    for (const auto &obj : s57s) {
        if (isPointObject(obj)) {
            n++;
        }
    }
    return n;
}

void loadBuiltUpAreas(ChartData::Builder &root, const S57Vector &src)
{

    // Built-Up areas actaully have either polygons OR points. Those two variants
    // are handled independently below.

    int n = countPolygonObjects(src);
    if (n > 0) {
        auto dst = root.initBuiltUpAreas(static_cast<unsigned int>(n));
        unsigned int i = 0;
        for (const auto &obj : src) {
            if (!isPolygonObject(obj)) {
                continue;
            }
            auto dstElement = dst[i++];
            auto name = obj->attribute<std::string>(oesenc::S57::Attribute::ObjectName);
            if (name.has_value()) {
                dstElement.setName(name.value());
            }

            loadPolygonsFromS57<ChartData::BuiltUpArea>(dstElement, obj);
            computeCentroidFromPolygons<ChartData::BuiltUpArea>(dstElement);
        }
    }

    n = countPointObjects(src);
    if (n > 0) {
        auto dst = root.initBuiltUpPoints(static_cast<unsigned int>(n));
        unsigned int i = 0;
        for (const auto &obj : src) {
            if (!isPointObject(obj)) {
                continue;
            }
            auto dstElement = dst[i++];
            auto name = obj->attribute<std::string>(oesenc::S57::Attribute::ObjectName);
            auto point = obj->pointGeometry();
            if (point.has_value() && name.has_value()) {
                dstElement.setName(name.value());
                auto pos = dstElement.getPosition();
                pos.setLatitude(point.value().latitude());
                pos.setLongitude(point.value().longitude());
            }
        }
    }
}

void loadCoastLines(ChartData::Builder &root, const S57Vector &src)
{
    auto coastLines = root.initCoastLines(static_cast<unsigned int>(src.size()));

    int i = 0;
    for (const oesenc::S57 *s57 : src) {
        loadLinesFromS57<ChartData::CoastLine>(coastLines[i++], s57);
    }
}

void loadDepthContours(ChartData::Builder &root, const S57Vector &src)
{
    auto depthContours = root.initDepthContours(static_cast<unsigned int>(src.size()));

    int i = 0;
    for (const oesenc::S57 *s57 : src) {
        loadLinesFromS57<ChartData::DepthContour>(depthContours[i++], s57);
    }
}

void loadCoverage(ChartData::Builder &root, const S57Vector &src)
{
    std::vector<const oesenc::S57 *> coverageRecords;

    for (const oesenc::S57 *s57 : src) {
        auto categoryOfCoverage = s57->attribute<uint32_t>(oesenc::S57::Attribute::CategoryOfCoverage);
        if (categoryOfCoverage.has_value() && categoryOfCoverage.value() == 1) {
            coverageRecords.push_back(s57);
        }
    }

    auto coverages = root.initCoverage(static_cast<unsigned int>(coverageRecords.size()));

    int i = 0;
    for (const auto &obj : coverageRecords) {
        ChartData::CoverageArea::Builder coverage = coverages[i++];
        loadPolygonsFromS57<ChartData::CoverageArea>(coverage, obj);
    }
}

void loadDepthAreas(ChartData::Builder &root, const S57Vector &src)
{
    auto dst = root.initDepthAreas(static_cast<unsigned int>(src.size()));

    unsigned int i = 0;
    for (const oesenc::S57 *s57 : src) {
        auto dstArea = dst[i++];
        // Should read both limits here
        auto depth = s57->attribute<float>(oesenc::S57::Attribute::DepthValue1);
        if (depth.has_value()) {
            dstArea.setDepth(depth.value());
        }

        loadPolygonsFromS57<ChartData::DepthArea>(dstArea, s57);
    }
}

void loadLandRegions(ChartData::Builder &root, S57Vector &src)
{
    auto dst = root.initLandRegions(static_cast<unsigned int>(src.size()));
    unsigned int i = 0;
    for (const auto &obj : src) {
        auto dstElement = dst[i++];
        auto name = obj->attribute<std::string>(oesenc::S57::Attribute::ObjectName);
        auto point = obj->pointGeometry();

        if (point.has_value() && name.has_value()) {
            dstElement.setName(name.value());
            auto pos = dstElement.getPosition();
            pos.setLatitude(point.value().latitude());
            pos.setLongitude(point.value().longitude());
        }
    }
}

struct Sounding
{
    Pos pos;
    double depth = 0;
    bool hasValue = false;
};

void loadSoundings(ChartData::Builder &root, const S57Vector &objs)
{
    // Soundings are "multi point geometries" which means that there can be
    // multiple soundings (depth + position) per S57 object. The S57 object may
    // have other attributes as well.

    // Ignore any S57 attributes and flatten the soundings to a flat list
    // of position and depth using the internal ChartArea::Sounding struct.

    std::vector<Sounding> soundings;

    for (const auto &obj : objs) {
        for (const auto &sounding : obj->multiPointGeometry()) {
            soundings.push_back(Sounding { { sounding.position.latitude(),
                                             sounding.position.longitude() },
                                           sounding.value });
        }
    }

    auto dst = root.initSoundings(static_cast<unsigned int>(soundings.size()));

    unsigned int i = 0;
    for (const auto &sounding : soundings) {
        auto dstItem = dst[i++];
        auto pos = dstItem.getPosition();
        pos.setLatitude(sounding.pos.lat());
        pos.setLongitude(sounding.pos.lon());
        dstItem.setDepth(sounding.depth);
    }
}

void loadBeacons(ChartData::Builder &root, const S57Vector &src)
{
    auto dst = root.initBeacons(static_cast<unsigned int>(src.size()));

    unsigned int i = 0;
    for (const auto &obj : src) {
        auto dstItem = dst[i++];
        auto name = obj->attribute<std::string>(oesenc::S57::Attribute::ObjectName);
        auto shape = obj->attribute<uint32_t>(oesenc::S57::Attribute::BeaconShape);
        auto position = obj->pointGeometry();

        if (position.has_value() && name.has_value() && shape.has_value()) {
            auto dstPos = dstItem.getPosition();
            dstPos.setLatitude(position->latitude());
            dstPos.setLongitude(position->longitude());
            dstItem.setName(name.value());
            switch (shape.value()) {
            case 1:
                dstItem.setShape(ChartData::BeaconShape::STAKE);
                break;
            case 2:
                dstItem.setShape(ChartData::BeaconShape::WITHY);
                break;
            case 3:
                dstItem.setShape(ChartData::BeaconShape::BEACON_TOWER);
                break;
            case 4:
                dstItem.setShape(ChartData::BeaconShape::LATTICE_BEACON);
                break;
            case 5:
                dstItem.setShape(ChartData::BeaconShape::PILE_BEACON);
                break;
            case 6:
                dstItem.setShape(ChartData::BeaconShape::CAIRN);
                break;
            case 7:
                dstItem.setShape(ChartData::BeaconShape::BOYANT);
                break;
            }
        }
    }
}

void loadUnderwaterRocks(ChartData::Builder &root, const S57Vector &src)
{
    auto dst = root.initUnderwaterRocks(static_cast<unsigned int>(src.size()));

    unsigned int i = 0;
    for (const auto &obj : src) {
        auto dstItem = dst[i++];
        auto waterLevelEffect = obj->attribute<uint32_t>(oesenc::S57::Attribute::WaterLevelEffect);
        auto valueOfSounding = obj->attribute<float>(oesenc::S57::Attribute::ValueOfSounding);
        auto position = obj->pointGeometry();

        if (waterLevelEffect.has_value() && valueOfSounding.has_value() && position.has_value()) {
            auto dstPos = dstItem.getPosition();
            dstPos.setLatitude(position->latitude());
            dstPos.setLongitude(position->longitude());
            dstItem.setDepth(valueOfSounding.value());

            switch (waterLevelEffect.value()) {
            case 1:
                dstItem.setWaterlevelEffect(ChartData::WaterLevelEffect::PARTLY_SUBMERGED_AT_HIGH_WATER);
                break;
            case 2:
                dstItem.setWaterlevelEffect(ChartData::WaterLevelEffect::ALWAYS_DRY);
                break;
            case 3:
                dstItem.setWaterlevelEffect(ChartData::WaterLevelEffect::ALWAYS_SUBMERGED);
                break;
            case 4:
                dstItem.setWaterlevelEffect(ChartData::WaterLevelEffect::COVERS_AND_UNCOVERS);
                break;
            case 5:
                dstItem.setWaterlevelEffect(ChartData::WaterLevelEffect::AWASH);
                break;
            case 6:
                dstItem.setWaterlevelEffect(ChartData::WaterLevelEffect::SUBJECT_TO_FLOODING);
                break;
            case 7:
                dstItem.setWaterlevelEffect(ChartData::WaterLevelEffect::FLOATING);
                break;
            }
        }
    }
}

void loadBuoyLateral(ChartData::Builder &root, const S57Vector &src)
{
    auto dst = root.initLateralBuoys(static_cast<unsigned int>(src.size()));

    unsigned int i = 0;
    for (const auto &obj : src) {
        auto dstItem = dst[i++];
        auto position = obj->pointGeometry();
        auto category = obj->attribute<uint32_t>(oesenc::S57::Attribute::CategoryOfLateralMark);
        auto shape = obj->attribute<uint32_t>(oesenc::S57::Attribute::BuoyShape);
        auto color = obj->attribute<std::string>(oesenc::S57::Attribute::Colour);

        if (position.has_value()
            && category.has_value()
            && shape.has_value()
            && color.has_value()) {

            auto dstPos = dstItem.getPosition();
            dstPos.setLatitude(position->latitude());
            dstPos.setLongitude(position->longitude());

            auto colorString = color.value();
            auto colorInt = color.value();

            switch (category.value()) {
            case 1:
                dstItem.setCategory(ChartData::CategoryOfLateralMark::PORT);
                break;
            case 2:
                dstItem.setCategory(ChartData::CategoryOfLateralMark::STARBOARD);
                break;
            case 3:
                dstItem.setCategory(ChartData::CategoryOfLateralMark::CHANNEL_TO_STARBOARD);
                break;
            case 4:
                dstItem.setCategory(ChartData::CategoryOfLateralMark::CHANNEL_TO_PORT);
                break;
            }

            switch (shape.value()) {
            case 1:
                dstItem.setShape(ChartData::BuoyShape::CONICAL);
                break;
            case 2:
                dstItem.setShape(ChartData::BuoyShape::CAN);
                break;
            case 3:
                dstItem.setShape(ChartData::BuoyShape::SPHERICAL);
                break;
            case 4:
                dstItem.setShape(ChartData::BuoyShape::PILLAR);
                break;
            case 5:
                dstItem.setShape(ChartData::BuoyShape::SPAR);
                break;
            case 6:
                dstItem.setShape(ChartData::BuoyShape::BARREL);
                break;
            case 7:
                dstItem.setShape(ChartData::BuoyShape::SUPER_BUOY);
                break;
            case 8:
                dstItem.setShape(ChartData::BuoyShape::ICE_BUOY);
                break;
            }

            if (color.value() == "1") {
                dstItem.setColor(ChartData::Color::WHITE);
            } else if (color.value() == "2") {
                dstItem.setColor(ChartData::Color::BLACK);
            } else if (color.value() == "3") {
                dstItem.setColor(ChartData::Color::RED);
            } else if (color.value() == "4") {
                dstItem.setColor(ChartData::Color::GREEN);
            } else if (color.value() == "5") {
                dstItem.setColor(ChartData::Color::BLUE);
            } else if (color.value() == "6") {
                dstItem.setColor(ChartData::Color::YELLOW);
            } else if (color.value() == "7") {
                dstItem.setColor(ChartData::Color::GREY);
            } else if (color.value() == "8") {
                dstItem.setColor(ChartData::Color::BROWN);
            } else if (color.value() == "9") {
                dstItem.setColor(ChartData::Color::AMBER);
            } else if (color.value() == "10") {
                dstItem.setColor(ChartData::Color::VIOLET);
            } else if (color.value() == "11") {
                dstItem.setColor(ChartData::Color::ORANGE);
            } else if (color.value() == "12") {
                dstItem.setColor(ChartData::Color::MAGENTA);
            } else if (color.value() == "13") {
                dstItem.setColor(ChartData::Color::PINK);
            } else {
                std::cerr << "Unable to parse color: " << color.value() << " (multiple colors?)" << std::endl;
            }
        }
    }
}

void loadPontoons(ChartData::Builder &root, const S57Vector &src)
{
    auto dst = root.initPontoons(static_cast<unsigned int>(src.size()));

    unsigned int i = 0;
    for (const oesenc::S57 *s57 : src) {
        ChartData::Pontoon::Builder pontoon = dst[i++];

        auto name = s57->attribute<std::string>(oesenc::S57::Attribute::ObjectName);
        if (name.has_value()) {
            pontoon.setName(name.value());
        }

        loadPolygonsFromS57<ChartData::Pontoon>(pontoon, s57);
        loadLinesFromS57<ChartData::Pontoon>(pontoon, s57);
    }
}

void loadRoads(ChartData::Builder &root, const S57Vector &src)
{
    auto dst = root.initRoads(static_cast<unsigned int>(src.size()));

    int i = 0;
    for (const oesenc::S57 *s57 : src) {
        ChartData::Road::Builder dstItem = dst[i++];

        loadPolygonsFromS57<ChartData::Road>(dstItem, s57);
        loadLinesFromS57<ChartData::Road>(dstItem, s57);

        std::optional<std::string> name = s57->attribute<std::string>(oesenc::S57::Attribute::ObjectName);
        auto categoryOfRoad = s57->attribute<uint32_t>(oesenc::S57::Attribute::CategoryOfRoad);

        if (name.has_value()) {
            dstItem.setName(name.value());
        }

        if (categoryOfRoad.has_value()) {
            switch (categoryOfRoad.value()) {
            case 1:
                dstItem.setCategory(::ChartData::CategoryOfRoad::MOTORWAY);
                break;
            case 2:
                dstItem.setCategory(::ChartData::CategoryOfRoad::MAJOR_ROAD);
                break;
            case 3:
                dstItem.setCategory(::ChartData::CategoryOfRoad::MINOR_ROAD);
                break;
            case 4:
                dstItem.setCategory(::ChartData::CategoryOfRoad::TRACK);
                break;
            case 5:
                dstItem.setCategory(::ChartData::CategoryOfRoad::MAJOR_STREET);
                break;
            case 6:
                dstItem.setCategory(::ChartData::CategoryOfRoad::MINOR_STREET);
                break;
            case 7:
                dstItem.setCategory(::ChartData::CategoryOfRoad::CROSSING);
                break;
            }
        }
    }
}

Pos calcAveragePosition(const capnp::List<ChartData::Position>::Reader &positions)
{
    double avgLat = 0;
    double avgLon = 0;
    int nPositions = 0;
    for (const auto &pos : positions) {
        avgLat += pos.getLatitude();
        avgLon += pos.getLongitude();
        nPositions++;
    }

    if (nPositions == 0) {
        return Pos();
    }
    return Pos(avgLat / nPositions, avgLon / nPositions);
}

}

std::unique_ptr<capnp::MallocMessageBuilder>
Chart::buildFromS57(const std::vector<oesenc::S57> &objects,
                    const GeoRect &boundingBox,
                    const std::string &name,
                    int scale)
{
    auto message = std::make_unique<capnp::MallocMessageBuilder>();
    ChartData::Builder root = message->initRoot<ChartData>();
    std::unordered_map<oesenc::S57::Type, std::vector<const oesenc::S57 *>> sortedObjects;

    for (const oesenc::S57 &obj : objects) {
        if (sortedObjects.find(obj.type()) == sortedObjects.end()) {
            sortedObjects[obj.type()];
        }
        sortedObjects[obj.type()].push_back(&obj);
    }

    root.setNativeScale(scale);
    root.setName(name);

    ChartData::Position::Builder topLeft = root.initTopLeft();
    topLeft.setLatitude(boundingBox.top());
    topLeft.setLongitude(boundingBox.left());

    ChartData::Position::Builder bottomRight = root.initBottomRight();
    bottomRight.setLatitude(boundingBox.bottom());
    bottomRight.setLongitude(boundingBox.right());

    loadCoverage(root, sortedObjects[oesenc::S57::Type::Coverage]);
    loadCoastLines(root, sortedObjects[oesenc::S57::Type::CoastLine]);
    loadLandAreas(root, sortedObjects[oesenc::S57::Type::LandArea]);
    loadLandRegions(root, sortedObjects[oesenc::S57::Type::LandRegion]);
    loadDepthAreas(root, sortedObjects[oesenc::S57::Type::DepthArea]);
    loadDepthContours(root, sortedObjects[oesenc::S57::Type::DepthContour]);
    loadBuiltUpAreas(root, sortedObjects[oesenc::S57::Type::BuiltUpArea]);
    loadSoundings(root, sortedObjects[oesenc::S57::Type::Sounding]);
    loadRoads(root, sortedObjects[oesenc::S57::Type::Road]);
    loadBeacons(root, sortedObjects[oesenc::S57::Type::Beacon]);
    loadUnderwaterRocks(root, sortedObjects[oesenc::S57::Type::UnderwaterRock]);
    loadBuoyLateral(root, sortedObjects[oesenc::S57::Type::BuoyLateral]);
    loadPontoons(root, sortedObjects[oesenc::S57::Type::Pontoon]);

    return message;
}

Chart::~Chart()
{
    m_capnpReader.reset();
    if (m_file) {
        fclose(m_file);
    }
}

Chart::Chart(FILE *file)
    : m_file(file)
{
#ifdef Q_OS_WIN
    const int fd = _fileno(file);
#else
    const int fd = fileno(file);
#endif
    assert(fd);
    m_capnpReader = std::make_unique<::capnp::PackedFdMessageReader>(fd);
}

std::shared_ptr<Chart> Chart::open(const std::string &filename)
{
    FILE *file = nullptr;

#ifdef Q_OS_WIN
    fopen_s(&file, filename.c_str(), "rb");
#else
    file = fopen(filename.c_str(), "rb");
#endif

    if (file == 0) {
        std::cerr << "Unable to open file for reading: " << filename << std::endl;
        return {};
    }

    return std::shared_ptr<Chart>(new Chart(file));
}

GeoRect Chart::boundingBox() const
{
    ChartData::Position::Reader topLeft = root().getTopLeft();
    ChartData::Position::Reader bottomRight = root().getBottomRight();

    return GeoRect(topLeft.getLatitude(),
                   bottomRight.getLatitude(),
                   topLeft.getLongitude(),
                   bottomRight.getLongitude());
}

bool Chart::write(capnp::MallocMessageBuilder *message, const std::string &filename)
{
    FILE *file = 0;

#ifdef Q_OS_WIN
    fopen_s(&file, filename.c_str(), "wb");
#else
    file = fopen(filename.c_str(), "wb");
#endif
    if (!file) {
        std::cerr << "Failed to write file" << std::endl;
        return false;
    }

#ifdef Q_OS_WIN
    int fd = _fileno(file);
#else
    const int fd = fileno(file);
#endif
    capnp::writePackedMessageToFd(fd, *message);
    fclose(file);

    return true;
}

std::unique_ptr<capnp::MallocMessageBuilder> Chart::buildClipped(ChartClipper::Config config) const
{
    // Hack to ensure that resolution in clipper is high enough.
    config.latitudeResolution /= 10;
    config.longitudeResolution /= 10;

    // Ugly to add this here
    config.chartBoundingBox = boundingBox();

    auto message = std::make_unique<capnp::MallocMessageBuilder>();
    ChartData::Builder root = message->initRoot<ChartData>();

    root.setName(name());
    root.setNativeScale(nativeScale());

    clipPolygonItems<ChartData::CoverageArea>(
        coverage(),
        config,
        [&](unsigned int length) {
            return root.initCoverage(length);
        },
        {});

    clipPolygonItems<ChartData::LandArea>(
        landAreas(),
        config,
        [&](unsigned int length) {
            return root.initLandAreas(length);
        },
        [](ChartData::LandArea::Builder &dst, const ChartData::LandArea::Reader &src) {
            dst.setName(src.getName());
            dst.setCentroid(src.getCentroid());
        });

    clipPolygonItems<ChartData::BuiltUpArea>(
        builtUpAreas(),
        config,
        [&](unsigned int length) {
            return root.initBuiltUpAreas(length);
        },
        [](ChartData::BuiltUpArea::Builder &dst, const ChartData::BuiltUpArea::Reader &src) {
            dst.setCentroid(src.getCentroid());
            dst.setName(src.getName());
        });

    clipPointItems<ChartData::BuiltUpPoint>(
        builtUpPoints(),
        config,
        [&](unsigned int length) {
            return root.initBuiltUpPoints(length);
        },
        [](ChartData::BuiltUpPoint::Builder &dst, const ChartData::BuiltUpPoint::Reader &src) {
            dst.setName(src.getName());
            dst.setPosition(src.getPosition());
        });

    clipPointItems<ChartData::LandRegion>(
        landRegions(),
        config,
        [&](unsigned int length) {
            return root.initLandRegions(length);
        },
        [](ChartData::LandRegion::Builder &dst, const ChartData::LandRegion::Reader &src) {
            dst.setName(src.getName());
            dst.setPosition(src.getPosition());
        });

    clipPolygonItems<ChartData::DepthArea>(
        depthAreas(),
        config,
        [&](unsigned int length) {
            return root.initDepthAreas(length);
        },
        [](ChartData::DepthArea::Builder &dst, const ChartData::DepthArea::Reader &src) {
            dst.setDepth(src.getDepth());
        });

    clipLineItems<ChartData::DepthContour>(
        depthContours(),
        config,
        [&](unsigned int length) {
            return root.initDepthContours(length);
        },
        {});

    clipPointItems<ChartData::Sounding>(
        soundings(),
        config,
        [&](unsigned int length) {
            return root.initSoundings(length);
        },
        [](ChartData::Sounding::Builder &dst, const ChartData::Sounding::Reader &src) {
            dst.setPosition(src.getPosition());
            dst.setDepth(src.getDepth());
        });

    clipPointItems<ChartData::Beacon>(
        beacons(),
        config,
        [&](unsigned int length) {
            return root.initBeacons(length);
        },
        [](ChartData::Beacon::Builder &dst, const ChartData::Beacon::Reader &src) {
            dst.setName(src.getName());
            dst.setPosition(src.getPosition());
            dst.setShape(src.getShape());
        });

    clipPointItems<ChartData::UnderwaterRock>(
        underwaterRocks(),
        config,
        [&](unsigned int length) {
            return root.initUnderwaterRocks(length);
        },
        [](ChartData::UnderwaterRock::Builder &dst, const ChartData::UnderwaterRock::Reader &src) {
            dst.setDepth(src.getDepth());
            dst.setPosition(src.getPosition());
            dst.setWaterlevelEffect(src.getWaterlevelEffect());
        });

    clipPointItems<ChartData::BuoyLateral>(
        lateralBuoys(),
        config,
        [&](unsigned int length) {
            return root.initLateralBuoys(length);
        },
        [](ChartData::BuoyLateral::Builder &dst, const ChartData::BuoyLateral::Reader &src) {
            dst.setPosition(src.getPosition());
            dst.setCategory(src.getCategory());
            dst.setShape(src.getShape());
            dst.setColor(src.getColor());
        });

    clipLineItems<ChartData::CoastLine>(
        coastLines(),
        config,
        [&](unsigned int length) {
            return root.initCoastLines(length);
        },
        {});

    clipPolygonOrLineItems<ChartData::Pontoon>(
        pontoons(),
        config,
        [&](unsigned int length) {
            return root.initPontoons(length);
        },
        [](ChartData::Pontoon::Builder &dst, const ChartData::Pontoon::Reader &src) {
            dst.setName(src.getName());
        });

    clipPolygonOrLineItems<ChartData::Road>(
        roads(),
        config,
        [&](unsigned int length) {
            return root.initRoads(length);
        },
        [](ChartData::Road::Builder &dst, const ChartData::Road::Reader &src) {
            dst.setCategory(src.getCategory());
            dst.setName(src.getName());
        });

    return message;
}
