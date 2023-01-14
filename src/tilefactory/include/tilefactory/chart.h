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

#include "tilefactory_export.h"

class TILEFACTORY_EXPORT Chart
{
public:
    struct Sounding
    {
        Pos pos;
        double depth = 0;
        bool hasValue = false;
    };

    static std::shared_ptr<Chart> open(const std::string &filename);
    static bool write(capnp::MallocMessageBuilder *message, const std::string &filename);
    static std::unique_ptr<capnp::MallocMessageBuilder>
    buildFromS57(const std::vector<oesenc::S57> &objects,
                 const GeoRect &boundingBox,
                 const std::string &name,
                 int scale);

    Chart() = delete;
    ~Chart();
    Chart(Chart &&) = delete;
    Chart(const Chart &) = delete;

    std::unique_ptr<capnp::MallocMessageBuilder> buildClipped(ChartClipper::Config config) const;

    static uint64_t typeId() { return ChartData::_capnpPrivate::typeId; }

    ChartData::Reader root() const
    {
        assert(m_capnpReader);
        return m_capnpReader->getRoot<ChartData>();
    }

    int nativeScale() const { return root().getNativeScale(); }
    std::string name() const { return root().getName(); }
    GeoRect boundingBox() const;
    capnp::List<ChartData::CoverageArea>::Reader coverage() const { return root().getCoverage(); }
    capnp::List<ChartData::LandArea>::Reader landAreas() const { return root().getLandAreas(); }
    capnp::List<ChartData::DepthArea>::Reader depthAreas() const { return root().getDepthAreas(); }
    capnp::List<ChartData::BuiltUpArea>::Reader builtUpAreas() const { return root().getBuiltUpAreas(); }
    capnp::List<ChartData::BuiltUpPoint>::Reader builtUpPoints() const { return root().getBuiltUpPoints(); }
    capnp::List<ChartData::LandRegion>::Reader landRegions() const { return root().getLandRegions(); }
    capnp::List<ChartData::Sounding>::Reader soundings() const { return root().getSoundings(); }
    capnp::List<ChartData::Beacon>::Reader beacons() const { return root().getBeacons(); }
    capnp::List<ChartData::UnderwaterRock>::Reader underwaterRocks() const { return root().getUnderwaterRocks(); }
    capnp::List<ChartData::BuoyLateral>::Reader lateralBuoys() const { return root().getLateralBuoys(); }
    capnp::List<ChartData::Road>::Reader roads() const { return root().getRoads(); }

private:
    Chart(FILE *fd);

    using S57Vector = const std::vector<const oesenc::S57 *>;

    static void loadCoverage(ChartData::Builder &root, S57Vector &src);
    static void loadLandAreas(ChartData::Builder &root, S57Vector &src);
    static void loadDepthAreas(ChartData::Builder &root, S57Vector &objs);
    static void loadBuiltUpAreas(ChartData::Builder &root, S57Vector &objs);
    static void loadSoundings(ChartData::Builder &root, S57Vector &objs);
    static void loadLandRegions(ChartData::Builder &root, S57Vector &objs);
    static void loadBeacons(ChartData::Builder &root, S57Vector &objs);
    static void loadUnderwaterRocks(ChartData::Builder &root, S57Vector &objs);
    static void loadRoads(ChartData::Builder &root, S57Vector &objs);
    static void loadBuoyLateral(ChartData::Builder &root, S57Vector &src);
    static Pos calcAveragePosition(const capnp::List<ChartData::Position>::Reader &positions);
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

    template <typename T>
    struct ClippedPolygonItem
    {
        std::vector<ChartClipper::Polygon> polygons;
        typename T::Reader item;
    };

    template <typename T>
    static void clipPolygonItems(const typename capnp::List<T>::Reader &src,
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
    static void copyPolygonsToBuilder(typename T::Builder builder,
                                      const std::vector<ChartClipper::Polygon> &polygons);

    static inline void fromOesencPolygon(ChartData::Polygon::Builder dst,
                                         const oesenc::S57::MultiGeometry &srcPolygon);

    template <typename T>
    static void loadPolygonsFromS57(typename T::Builder dst, const oesenc::S57 *s57);

    template <typename T>
    static void loadLinesFromS57(typename T::Builder dst, const oesenc::S57 *s57);

    static std::vector<ChartClipper::Polygon> clipPolygons(const capnp::List<ChartData::Polygon>::Reader &polygons,
                                                           const ChartClipper::Config &config);

    template <typename T>
    static void computeCentroidFromPolygons(typename T::Builder builder);

    static inline void fromOesencPosToCapnp(capnp::List<ChartData::Position>::Builder &dst,
                                            const std::vector<oesenc::Position> &src);

    using Polygon = std::vector<Pos>;
    static inline void toCapnPolygons(capnp::List<capnp::List<ChartData::Position>>::Builder dst,
                                      const std::vector<Polygon> &src);
    static inline void toCapnPolygon(capnp::List<ChartData::Position>::Builder dst, const Polygon &src);

    std::unique_ptr<::capnp::PackedFdMessageReader> m_capnpReader;
    FILE *m_file = nullptr;
};
