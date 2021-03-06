#include <chrono>
#include <thread>

#include "tilefactory/chart.h"
#include "tilefactory/georect.h"
#include "tilefactory/mercator.h"

Chart::Chart()
{
    m_message = std::make_unique<capnp::MallocMessageBuilder>();
}

Chart::~Chart()
{
}

Chart::Chart(const std::vector<oesenc::S57> &objects,
             const GeoRect &boundingBox,
             const std::string &name,
             int scale)
    : m_boundingBox(boundingBox)
{
    m_message = std::make_unique<capnp::MallocMessageBuilder>();
    auto root = m_message->initRoot<ChartData>();
    std::unordered_map<oesenc::S57::Type, std::vector<const oesenc::S57 *>> sortedObjects;

    for (const oesenc::S57 &obj : objects) {
        if (sortedObjects.find(obj.type()) == sortedObjects.end()) {
            sortedObjects[obj.type()];
        }
        sortedObjects[obj.type()].push_back(&obj);
    }

    root.setNativeScale(scale);
    root.setName(name);
    loadCoverage(root, sortedObjects[oesenc::S57::Type::Coverage]);
    loadLandAreas(root, sortedObjects[oesenc::S57::Type::LandArea]);
    loadDepthAreas(root, sortedObjects[oesenc::S57::Type::DepthArea]);
    loadBuiltUpAreas(root, sortedObjects[oesenc::S57::Type::BuiltUpArea]);
    loadSoundings(root, sortedObjects[oesenc::S57::Type::Sounding]);
    loadRoads(root, sortedObjects[oesenc::S57::Type::Road]);
    loadBeacons(root, sortedObjects[oesenc::S57::Type::Beacon]);
    loadUnderwaterRocks(root, sortedObjects[oesenc::S57::Type::UnderwaterRock]);
    loadBuoyLateral(root, sortedObjects[oesenc::S57::Type::BuoyLateral]);
}

Chart::Chart(std::unique_ptr<capnp::MallocMessageBuilder> messageBuilder)
    : m_message(std::move(messageBuilder))
{
}

Chart::Chart(const std::string &filename)
{
    read(filename);
}

void Chart::read(const std::string &filename)
{
    FILE *file = nullptr;

#ifdef Q_OS_WIN
    fopen_s(&file, filename.c_str(), "rb");
#else
    file = fopen(filename.c_str(), "rb");
#endif

    if (file == 0) {
        std::cerr << "Unable to open file " << filename << std::endl;
        return;
    }

#ifdef Q_OS_WIN
    const int fd = _fileno(file);
#else
   const int fd = fileno(file);
#endif
    if (fd == 0) {
        std::cerr << "Unable to open file " << filename << std::endl;
        return;
    }

    // Create a reader here and copy the data to the message builder so it has
    // no limits on number of reads (see security section of the docs).
    ::capnp::PackedFdMessageReader reader(fd);
    m_message = std::make_unique<::capnp::MallocMessageBuilder>();
    reader.getRoot<ChartData>();
    m_message->setRoot(reader.getRoot<ChartData>());
    fclose(file);
}

void Chart::write(const std::string &filename)
{
    FILE *file = 0;

#ifdef Q_OS_WIN
    fopen_s(&file, filename.c_str(), "wb");
#else
    file = fopen(filename.c_str(), "wb");
#endif
    if (!file) {
        std::cerr << "Failed to write file" << std::endl;
        return;
    }

#ifdef Q_OS_WIN
    int fd = _fileno(file);
#else
    const int fd = fileno(file);
#endif

    capnp::writePackedMessageToFd(fd, *m_message.get());
    fclose(file);
}

bool Chart::pointsAround(const capnp::List<ChartData::Position>::Reader &positions,
                         const GeoRect &rect)
{
    bool left = false;
    bool right = false;
    bool above = false;
    bool below = false;
    double latMargin = 0;
    double lonMargin = 0;

    for (const auto &pos : positions) {
        if (rect.contains(pos.getLatitude(), pos.getLongitude())) {
            return false;
        }

        if (pos.getLongitude() < rect.left() + lonMargin) {
            left = true;
        } else if (pos.getLongitude() > rect.right() - lonMargin) {
            right = true;
        }

        if (pos.getLatitude() > rect.top() - latMargin) {
            above = true;
        } else if (pos.getLatitude() < rect.bottom() + latMargin) {
            below = true;
        }
    }

    return left && right && above && below;
}

Chart Chart::clipped(ChartClipper::Config config, SoundingCache *soundingCache) const
{
    // Ugly to add this here
    config.chartBoundingBox = m_boundingBox;

    auto message = std::make_unique<capnp::MallocMessageBuilder>();
    ChartData::Builder root = message->initRoot<ChartData>();

    root.setName(name());
    config.moveOutEdges = false;
    root.setNativeScale(nativeScale());

    bool empty = clipPolygonItems<ChartData::Area>(
        coverage(),
        config,
        [&](unsigned int length) {
            return root.initCoverage(length);
        },
        {});

    root.setCoverageType(ChartData::CoverageType::PARTIAL);

    if (empty) {
        root.setCoverageType(ChartData::CoverageType::ZERO);
    } else {
        if (root.getCoverage().size() == 1) {
            const auto &polygons = root.getCoverage()[0].getPolygons();
            if (polygons.size() == 1) {
                const auto &polygon = polygons.asReader()[0];
                GeoRect test(config.box.top(), config.box.bottom(), config.box.left(), config.box.right());
                if (pointsAround(polygon.getPositions(), test)) {
                    root.setCoverageType(ChartData::CoverageType::FULL);
                }
            }
        }
    }

    config.moveOutEdges = true;

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

    clipPolygonItems<ChartData::DepthArea>(
        depthAreas(),
        config,
        [&](unsigned int length) {
            return root.initDepthAreas(length);
        },
        [](ChartData::DepthArea::Builder &dst, const ChartData::DepthArea::Reader &src) {
            dst.setDepth(src.getDepth());
        });

    std::vector<Sounding> decimated;

    if (soundingCache) {
        soundingCache->mutex.lock();
        auto it = soundingCache->soundings.find(config.maxPixelsPerLongitude);
        if (it != soundingCache->soundings.end()) {
            decimated = it->second;
        } else {
            decimated = decimateSoundings(config.maxPixelsPerLongitude);
        }
        soundingCache->soundings[config.maxPixelsPerLongitude] = decimated;
        soundingCache->mutex.unlock();
    } else {
        decimated = decimateSoundings(config.maxPixelsPerLongitude);
    }
    clipAndInsertSoundings(root, decimated, config);

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

    clipLineItems<ChartData::Road>(
        roads(),
        config,
        [&](unsigned int length) {
            return root.initRoads(length);
        },
        [](ChartData::Road::Builder &dst, const ChartData::Road::Reader &src) {
            dst.setCategory(src.getCategory());
            dst.setName(src.getName());
        });

    Chart clipped(std::move(message));
    return clipped;
}

void Chart::clipAndInsertSoundings(ChartData::Builder &root,
                                   const std::vector<Sounding> &src,
                                   const ChartClipper::Config &config)
{
    const GeoRect &box = config.box;

    GeoRect clipRect(box.top() + config.latitudeMargin,
                     box.bottom() - config.latitudeMargin,
                     box.left() - config.longitudeMargin,
                     box.right() + config.longitudeMargin);

    std::vector<Sounding> clipped;

    for (const auto &sounding : src) {
        if (!sounding.hasValue) {
            continue;
        }
        if (clipRect.contains(sounding.pos.lat(), sounding.pos.lon())) {
            clipped.push_back(sounding);
        }
    }

    auto dst = root.initSoundings(static_cast<unsigned int>(clipped.size()));

    int i = 0;
    for (const auto &sounding : clipped) {
        auto dstItem = dst[i++];
        auto pos = dstItem.getPosition();
        pos.setLatitude(sounding.pos.lat());
        pos.setLongitude(sounding.pos.lon());
        dstItem.setDepth(sounding.depth);
    }
}

template <typename T>
void Chart::clipPointItems(const typename capnp::List<T>::Reader &src,
                           ChartClipper::Config config,
                           std::function<typename capnp::List<T>::Builder(unsigned int length)> init,
                           std::function<void(typename T::Builder &, const typename T::Reader &)> copyFunction)
{
    std::vector<ClippedPointItem<T>> clipped;

    GeoRect clipRect(config.box.top() + config.latitudeMargin,
                     config.box.bottom() - config.latitudeMargin,
                     config.box.left() - config.longitudeMargin,
                     config.box.right() + config.longitudeMargin);

    for (const auto &element : src) {
        const Pos pos(element.getPosition().getLatitude(), element.getPosition().getLongitude());
        if (clipRect.contains(pos.lat(), pos.lon())) {
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
bool Chart::clipPolygonItems(const typename capnp::List<T>::Reader &src,
                             ChartClipper::Config config,
                             std::function<typename capnp::List<T>::Builder(unsigned int length)> init,
                             std::function<void(typename T::Builder &, const typename T::Reader &)> copyFunction)
{
    std::vector<ClippedPolygonItem<T>> clipped;

    bool empty = true;
    for (const auto &element : src) {
        if (element.getPolygons().size() == 0) {
            continue;
        }
        auto clippedPolygons = ChartClipper::clipPolygons(element.getPolygons(), config);
        if (clippedPolygons.empty()) {
            continue;
        }
        empty = false;
        ClippedPolygonItem<T> item;
        item.polygons = clippedPolygons;
        item.item = element;
        clipped.push_back(item);
    }

    auto finalTarget = init(static_cast<unsigned int>(clipped.size()));
    fillList<T>(finalTarget, clipped, copyFunction);
    return empty;
}

template <typename T>
void Chart::clipLineItems(const typename capnp::List<T>::Reader &src,
                          ChartClipper::Config config,
                          std::function<typename capnp::List<T>::Builder(unsigned int length)> init,
                          std::function<void(typename T::Builder &, const typename T::Reader &)> copyFunction)
{
    std::vector<ClippedLineItem<T>> clipped;
    const GeoRect &box = config.box;

    // Add a bit more clipping margin for lines since we're not clipping with
    // polyclipping library.
    GeoRect clipRect(box.top() + config.latitudeMargin * 5,
                     box.bottom() - config.latitudeMargin * 5,
                     box.left() - config.longitudeMargin * 5,
                     box.right() + config.longitudeMargin * 5);

    for (const auto &element : src) {

        std::vector<std::vector<Pos>> lines;
        auto srcLines = element.getLines();

        for (const auto &srcLine : srcLines) {
            Pos previous(0, 0);
            std::vector<Pos> line;

            for (const auto &srcPos : srcLine.getPositions()) {
                const Pos pos(srcPos.getLatitude(), srcPos.getLongitude());
                bool distanceThrehold = fabs(pos.lat() - previous.lat()) > config.latitudeResolution
                    || fabs(pos.lon() - previous.lon()) > config.longitudeResolution;

                if (clipRect.contains(srcPos.getLatitude(), srcPos.getLongitude()) && distanceThrehold) {
                    line.push_back(pos);
                    previous = pos;
                }
            }

            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        if (!lines.empty()) {
            clipped.push_back(ClippedLineItem<T> { lines, element });
        }
    }

    auto dst = init(static_cast<unsigned int>(clipped.size()));

    int i = 0;
    for (const auto &item : clipped) {
        auto element = dst[i++];
        // element.getPosition().setLatitude(item.pos.latitude());
        // element.getPosition().setLongitude(item.pos.longitude());
        auto dstLines = element.initLines(static_cast<unsigned int>(item.lines.size()));
        int i = 0;
        for (const auto &line : item.lines) {
            auto dstLine = dstLines[i++];
            auto positions = dstLine.initPositions(static_cast<unsigned int>(line.size()));
            int i = 0;
            for (const auto &pos : line) {
                auto dstPos = positions[i++];
                dstPos.setLatitude(pos.lat());
                dstPos.setLongitude(pos.lon());
            }
        }
        copyFunction(element, item.item);
    }
}

template <typename T>
void Chart::fillList(typename capnp::List<T>::Builder &dst,
                     const std::vector<Chart::ClippedPolygonItem<T>> &src,
                     std::function<void(typename T::Builder &, const typename T::Reader &)> copyFunction)
{
    int i = 0;
    for (const auto &item : src) {
        auto element = dst[i++];
        auto polygons = element.initPolygons((unsigned int)item.polygons.size());
        toCapnPolygons(polygons, item.polygons);
        if (copyFunction) {
            copyFunction(element, item.item);
        }
    }
}

void Chart::loadLandAreas(ChartData::Builder &root, S57Vector &src)
{
    auto landAreas = root.initLandAreas(static_cast<unsigned int>(src.size()));

    unsigned int i = 0;
    for (const auto &obj : src) {
        auto dstElement = landAreas[i++];
        auto name = obj->attribute<std::string>(oesenc::S57::Attribute::ObjectName);
        if (name.has_value()) {
            dstElement.setName(name.value());
        }
        auto dstPolygons = dstElement.initPolygons(static_cast<unsigned int>(obj->polygons().size()));
        fromOesencPolygons(dstPolygons, obj->polygons());

        // Not strictly a centroid, but easier to compute the average.
        Pos centroidPos = calcAveragePosition(dstPolygons.asReader());
        auto centroid = dstElement.getCentroid();
        centroid.setLatitude(centroidPos.lat());
        centroid.setLongitude(centroidPos.lon());
    }
}

bool Chart::isPolygonObject(const oesenc::S57 *obj)
{
    return !obj->polygons().empty();
}

bool Chart::isPointObject(const oesenc::S57 *obj)
{
    return obj->pointGeometry().has_value();
}

int Chart::countPolygonObjects(S57Vector &s57s)
{
    int n = 0;
    for (const auto &obj : s57s) {
        if (isPolygonObject(obj)) {
            n++;
        }
    }
    return n;
}

int Chart::countPointObjects(S57Vector &s57s)
{
    int n = 0;
    for (const auto &obj : s57s) {
        if (isPointObject(obj)) {
            n++;
        }
    }
    return n;
}

void Chart::loadBuiltUpAreas(ChartData::Builder &root, S57Vector &src)
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
            auto dstPolygons = dstElement.initPolygons(static_cast<unsigned int>(obj->polygons().size()));
            fromOesencPolygons(dstPolygons, obj->polygons());

            // Not strictly a centroid so will compute average instead
            Pos centroidPos = calcAveragePosition(dstPolygons.asReader());
            auto centroid = dstElement.getCentroid();
            centroid.setLatitude(centroidPos.lat());
            centroid.setLongitude(centroidPos.lon());
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

void Chart::loadCoverage(ChartData::Builder &root, S57Vector &src)
{
    std::vector<const oesenc::S57 *> coverageRecords;

    for (const auto &obj : src) {
        auto categoryOfCoverage = obj->attribute<uint32_t>(oesenc::S57::Attribute::CategoryOfCoverage);
        if (categoryOfCoverage.has_value() && categoryOfCoverage.value() == 1) {
            coverageRecords.push_back(obj);
        }
    }

    auto areas = root.initCoverage(static_cast<unsigned int>(coverageRecords.size()));

    unsigned int i = 0;
    for (const auto &obj : coverageRecords) {
        auto dstArea = areas[i++];
        auto dstPolygons = dstArea.initPolygons(static_cast<unsigned int>(obj->polygons().size()));
        fromOesencPolygons(dstPolygons, obj->polygons());
    }
}

void Chart::loadDepthAreas(ChartData::Builder &root, S57Vector &src)
{
    auto dst = root.initDepthAreas(static_cast<unsigned int>(src.size()));

    unsigned int i = 0;
    for (const auto &obj : src) {
        auto dstArea = dst[i++];
        auto dstPolygons = dstArea.initPolygons(static_cast<unsigned int>(obj->polygons().size()));
        // Should read both limits here
        auto depth = obj->attribute<float>(oesenc::S57::Attribute::DepthValue1);
        if (depth.has_value()) {
            dstArea.setDepth(depth.value());
        }
        fromOesencPolygons(dstPolygons, obj->polygons());
    }
}

void Chart::loadSoundings(ChartData::Builder &root, S57Vector &objs)
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

void Chart::loadBeacons(ChartData::Builder &root, S57Vector &src)
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

void Chart::loadUnderwaterRocks(ChartData::Builder &root, S57Vector &src)
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

void Chart::loadBuoyLateral(ChartData::Builder &root, S57Vector &src)
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

void Chart::loadRoads(ChartData::Builder &root, S57Vector &src)
{
    auto dst = root.initRoads(static_cast<unsigned int>(src.size()));

    int i = 0;
    for (const auto &obj : src) {
        auto dstItem = dst[i++];
        auto name = obj->attribute<std::string>(oesenc::S57::Attribute::ObjectName);
        auto categoryOfRoad = obj->attribute<uint32_t>(oesenc::S57::Attribute::CategoryOfRoad);
        auto lines = obj->lines();
        auto dstLines = dstItem.initLines(static_cast<unsigned int>(lines.size()));

        int i = 0;
        for (auto &line : lines) {
            auto dstLine = dstLines[i++];
            auto positions = dstLine.initPositions(static_cast<unsigned int>(line.size()));

            int i = 0;
            for (auto &position : line) {
                auto dstPos = positions[i++];
                dstPos.setLatitude(position.latitude());
                dstPos.setLongitude(position.longitude());
            }
        }

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

Pos Chart::calcAveragePosition(::capnp::List<ChartData::Polygon>::Reader polygons)
{
    double avgLat = 0;
    double avgLon = 0;
    int nPositions = 0;
    for (const auto &polygon : polygons) {
        for (const auto &pos : polygon.getPositions()) {
            avgLat += pos.getLatitude();
            avgLon += pos.getLongitude();
            nPositions++;
        }
    }

    if (nPositions == 0) {
        return Pos();
    }
    return Pos(avgLat / nPositions, avgLon / nPositions);
}

void Chart::toCapnPolygons(::capnp::List<ChartData::Polygon>::Builder dst,
                           const std::vector<Polygon> &src)
{
    int polygonIndex = 0;

    for (const Polygon &srcPolygon : src) {
        ChartData::Polygon::Builder p = dst[polygonIndex];
        auto dstPositions = p.initPositions((unsigned int)srcPolygon.size());

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

void Chart::fromOesencPolygons(::capnp::List<ChartData::Polygon>::Builder dst,
                               const std::vector<oesenc::S57::MultiGeometry> &src)
{
    int polygonIndex = 0;

    for (const auto &srcPolygon : src) {
        ChartData::Polygon::Builder p = dst[polygonIndex];
        auto dstPositions = p.initPositions((unsigned int)srcPolygon.size());

        int positionIndex = 0;
        for (const auto &pos : srcPolygon) {
            auto dstPos = dstPositions[positionIndex];
            dstPos.setLatitude(pos.latitude());
            dstPos.setLongitude(pos.longitude());
            positionIndex++;
        }

        polygonIndex++;
    }
}

void Chart::fromCapnPolygons(std::vector<Polygon> &dst,
                             const ::capnp::List<ChartData::Polygon>::Reader &src)
{
    dst.resize(src.size());
    int polygonIndex = 0;

    for (const ChartData::Polygon::Reader &polygon : src) {
        Polygon &dstPolygon = dst[polygonIndex];

        for (const ChartData::Position::Reader &position : polygon.getPositions()) {
            dstPolygon.push_back(Pos(position.getLatitude(),
                                     position.getLongitude()));
        }

        polygonIndex++;
    }
}

void Chart::toCapnPosition(ChartData::Position::Builder &dst, const Pos &src)
{
    dst.setLatitude(src.lat());
    dst.setLongitude(src.lon());
}

std::vector<Chart::Sounding> Chart::decimateSoundings(int maxPixelsPerLongitude) const
{
    const GeoRect &box = m_boundingBox;
    const double latitudeStep = box.top() - Mercator::mercatorHeightInverse(box.top(), 80, maxPixelsPerLongitude);
    const double longitudeStep = Mercator::mercatorWidthInverse(box.left(), 80, maxPixelsPerLongitude) - box.left();

    int horizBins = box.lonSpan() / longitudeStep;
    int vertBins = box.latSpan() / latitudeStep;

    // Risk of integer overflow. Convert to floating point.
    if (static_cast<double>(horizBins) * vertBins > 100000) {
        std::vector<Sounding> all;
        for (const auto &sounding : soundings()) {
            all.push_back(Sounding { { sounding.getPosition().getLatitude(),
                                       sounding.getPosition().getLongitude() },
                                     sounding.getDepth(),
                                     true });
        }
        return all;
    } else if (horizBins <= 0 || vertBins <= 0) {
        // Charts needs just one bin (probably zoomed way out)
        horizBins = 1;
        vertBins = 1;
    }

    std::vector<Sounding> bins;
    bins.resize(vertBins * horizBins);

    for (const auto &srcSounding : soundings()) {
        double lat = srcSounding.getPosition().getLatitude();
        double lon = srcSounding.getPosition().getLongitude();

        int horizontalBin = (lon - box.left()) / (box.right() - box.left()) * horizBins;
        int verticalBin = (lat - box.bottom()) / (box.top() - box.bottom()) * vertBins;

        horizontalBin = std::clamp(horizontalBin, 0, horizBins);
        verticalBin = std::clamp(verticalBin, 0, vertBins);

        Sounding sounding { { lat, lon }, srcSounding.getDepth(), true };

        int bin = horizontalBin + verticalBin * horizBins;
        if (bins[bin].hasValue) {
            if (sounding.depth < bins[bin].depth) {
                bins[bin] = sounding;
            }
        } else {
            bins[bin] = sounding;
        }
    }

    return bins;
}
