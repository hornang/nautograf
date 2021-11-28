#pragma once

#include <polyclipping/clipper.hpp>

#include "chartdata.capnp.h"
#include <capnp/message.h>

#include "tilefactory/georect.h"
#include "tilefactory/pos.h"

class ChartClipper
{
public:
    struct Config
    {
        GeoRect box;
        GeoRect chartBoundingBox;
        int maxPixelsPerLongitude = 0;
        float latitudeMargin = 0;
        float longitudeMargin = 0;
        float latitudeResolution = 0;
        float longitudeResolution = 0;
        bool moveOutEdges = false;
    };

    using Polygon = std::vector<Pos>;
    static std::vector<Polygon> clipPolygons(const capnp::List<ChartData::Polygon>::Reader &polygons,
                                             Config clipConfig);

private:
    static int inRange(double value, double min, double max, double margin);

    static std::vector<Polygon> moveOutChartEdges(const std::vector<Polygon> &polygons,
                                                       Config clipConfig);
    static inline ClipperLib::IntPoint toIntPoint(const Pos &pos,
                                                  const GeoRect &roi,
                                                  double xRes,
                                                  double yRes);
    static inline Pos fromIntPoint(const ClipperLib::IntPoint &intPoint, const GeoRect &clip, double xRes, double yRes);
    template <typename T>
    static std::vector<T> clipByPosition(const std::vector<T> &input, const GeoRect &rect);
};
