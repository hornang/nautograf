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
    capnp::List<ChartData::CoastLine>::Reader coastLines() const { return root().getCoastLines(); }
    capnp::List<ChartData::CoverageArea>::Reader coverage() const { return root().getCoverage(); }
    capnp::List<ChartData::LandArea>::Reader landAreas() const { return root().getLandAreas(); }
    capnp::List<ChartData::DepthArea>::Reader depthAreas() const { return root().getDepthAreas(); }
    capnp::List<ChartData::DepthContour>::Reader depthContours() const { return root().getDepthContours(); }
    capnp::List<ChartData::BuiltUpArea>::Reader builtUpAreas() const { return root().getBuiltUpAreas(); }
    capnp::List<ChartData::BuiltUpPoint>::Reader builtUpPoints() const { return root().getBuiltUpPoints(); }
    capnp::List<ChartData::LandRegion>::Reader landRegions() const { return root().getLandRegions(); }
    capnp::List<ChartData::Sounding>::Reader soundings() const { return root().getSoundings(); }
    capnp::List<ChartData::Beacon>::Reader beacons() const { return root().getBeacons(); }
    capnp::List<ChartData::UnderwaterRock>::Reader underwaterRocks() const { return root().getUnderwaterRocks(); }
    capnp::List<ChartData::BuoyLateral>::Reader lateralBuoys() const { return root().getLateralBuoys(); }
    capnp::List<ChartData::Pontoon>::Reader pontoons() const { return root().getPontoons(); }
    capnp::List<ChartData::ShorelineConstruction>::Reader shorelineConstructions() const { return root().getShorelineConstructions(); }
    capnp::List<ChartData::Road>::Reader roads() const { return root().getRoads(); }

private:
    Chart(FILE *fd);
    void read(const std::string &filename);
    std::unique_ptr<::capnp::PackedFdMessageReader> m_capnpReader;
    FILE *m_file = nullptr;
};
