#pragma once

#include <assert.h>
#include <memory>

#include "chartdata.capnp.h"
#include <capnp/dynamic.h>
#include <capnp/message.h>
#include <capnp/orphan.h>
#include <capnp/serialize-packed.h>

#include "oesenc/s57.h"
#include "tilefactory/chartclipper.h"
#include "tilefactory/georect.h"
#include "tilefactory/pos.h"

class Chart
{
public:
    struct Sounding
    {
        Pos pos;
        double depth = 0;
        bool hasValue = false;
    };

    Chart(std::unique_ptr<capnp::MallocMessageBuilder> messageBuilder);
    Chart();
    ~Chart();
    Chart(Chart &&) = default;
    Chart(const std::string &filename);
    Chart(const std::vector<oesenc::S57> &objects,
          const GeoRect &boundingBox,
          const std::string &name,
          int scale);

    static uint64_t typeId() { return ChartData::_capnpPrivate::typeId; }
    Chart clipped(ChartClipper::Config config) const;
    capnp::MallocMessageBuilder &messageBuilder();
    ::ChartData::Reader root() const { return m_message->getRoot<ChartData>().asReader(); }
    int nativeScale() const { return m_message->getRoot<ChartData>().getNativeScale(); }
    ::capnp::List<ChartData::Area>::Reader coverage() const { return m_message->getRoot<ChartData>().getCoverage().asReader(); }
    ::ChartData::CoverageType coverageType() const { return root().getCoverageType(); }
    ::capnp::List<ChartData::LandArea>::Reader landAreas() const { return m_message->getRoot<ChartData>().getLandAreas().asReader(); }
    ::capnp::List<ChartData::DepthArea>::Reader depthAreas() const { return m_message->getRoot<ChartData>().getDepthAreas().asReader(); }
    ::capnp::List<ChartData::BuiltUpArea>::Reader builtUpAreas() const { return m_message->getRoot<ChartData>().getBuiltUpAreas().asReader(); }
    ::capnp::List<ChartData::BuiltUpPoint>::Reader builtUpPoints() const { return root().getBuiltUpPoints(); }
    ::capnp::List<ChartData::Sounding>::Reader soundings() const { return m_message->getRoot<ChartData>().getSoundings().asReader(); }
    ::capnp::List<ChartData::Beacon>::Reader beacons() const { return m_message->getRoot<ChartData>().getBeacons().asReader(); }
    ::capnp::List<ChartData::UnderwaterRock>::Reader underwaterRocks() const { return m_message->getRoot<ChartData>().getUnderwaterRocks().asReader(); }
    ::capnp::List<ChartData::BuoyLateral>::Reader lateralBuoys() const { return m_message->getRoot<ChartData>().getLateralBuoys().asReader(); }
    ::capnp::List<ChartData::Road>::Reader roads() const { return m_message->getRoot<ChartData>().getRoads().asReader(); }
    const GeoRect &boundingBox() const { return m_boundingBox; }
    std::string name() const { return m_message->getRoot<ChartData>().getName().asReader(); }
    bool loaded() const { return m_message != nullptr; }
    void write(const std::string &filename);

private:
    using S57Vector = const std::vector<const oesenc::S57 *>;

    static void loadCoverage(ChartData::Builder &root, S57Vector &src);
    static void loadLandAreas(ChartData::Builder &root, S57Vector &src);
    static void loadDepthAreas(ChartData::Builder &root, S57Vector &objs);
    static void loadBuiltUpAreas(ChartData::Builder &root, S57Vector &objs);
    static void loadSoundings(ChartData::Builder &root, S57Vector &objs);
    static void loadBeacons(ChartData::Builder &root, S57Vector &objs);
    static void loadUnderwaterRocks(ChartData::Builder &root, S57Vector &objs);
    static void loadRoads(ChartData::Builder &root, S57Vector &objs);
    static void loadBuoyLateral(ChartData::Builder &root, S57Vector &src);
    static bool pointsAround(const capnp::List<ChartData::Position>::Reader &points, const GeoRect &box);
    static Pos calcAveragePosition(::capnp::List<ChartData::Polygon>::Reader dst);
    static inline int countPolygonObjects(S57Vector &s57);
    static inline int countPointObjects(S57Vector &s57);
    static inline bool isPolygonObject(const oesenc::S57 *obj);
    static inline bool isPointObject(const oesenc::S57 *obj);

    void read(const std::string &filename);

    template <typename T>
    struct ClippedPointItem
    {
        Pos pos;
        typename T::Reader item;
    };

    template <typename T>
    struct ClippedLineItem
    {
        std::vector<std::vector<Pos>> lines;
        typename T::Reader item;
    };

    using Polygon = std::vector<Pos>;

    template <typename T>
    struct ClippedPolygonItem
    {
        std::vector<Polygon> polygons;
        typename T::Reader item;
    };

    template <typename T>
    static bool clipPolygonItems(const typename capnp::List<T>::Reader &src,
                                 ChartClipper::Config config,
                                 std::function<typename capnp::List<T>::Builder(unsigned int length)> init,
                                 std::function<void(typename T::Builder &, const typename T::Reader &)> copyFunction);
    template <typename T>
    static void clipPointItems(const typename capnp::List<T>::Reader &src,
                               ChartClipper::Config config,
                               std::function<typename capnp::List<T>::Builder(unsigned int length)> init,
                               std::function<void(typename T::Builder &, const typename T::Reader &)> copyFunction);

    template <typename T>
    static void clipLineItems(const typename capnp::List<T>::Reader &src,
                              ChartClipper::Config config,
                              std::function<typename capnp::List<T>::Builder(unsigned int length)> init,
                              std::function<void(typename T::Builder &, const typename T::Reader &)> copyFunction);

    template <typename T>
    static void fillList(typename capnp::List<T>::Builder &dst,
                         const std::vector<ClippedPolygonItem<T>> &src,
                         std::function<void(typename T::Builder &, const typename T::Reader &)> copyFunction);

    static inline void fromCapnPolygons(std::vector<Polygon> &dst,
                                        const ::capnp::List<ChartData::Polygon>::Reader &src);
    static inline void fromOesencPolygons(::capnp::List<ChartData::Polygon>::Builder dst,
                                          const std::vector<oesenc::S57::MultiGeometry> &src);
    static inline void toCapnPosition(ChartData::Position::Builder &dst, const Pos &src);
    static inline void toCapnPolygons(::capnp::List<ChartData::Polygon>::Builder dst,
                                      const std::vector<Polygon> &src);

    GeoRect m_boundingBox;
    std::unique_ptr<::capnp::MallocMessageBuilder> m_message;
};
