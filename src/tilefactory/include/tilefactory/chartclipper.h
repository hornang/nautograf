#pragma once

#include <vector>

#include <polyclipping/clipper.hpp>

#include "chartdata.capnp.h"
#include "tilefactory/georect.h"
#include "tilefactory/pos.h"

class ChartClipper
{
public:
    using Line = std::vector<Pos>;

    struct Polygon
    {
        Line main;
        std::vector<Line> holes;
    };

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

    static std::vector<Polygon> clipPolygon(const ChartData::Polygon::Reader &polygon,
                                            Config clipConfig);
    static ClipperLib::Path toClipperPath(const capnp::List<ChartData::Position>::Reader &points,
                                          const GeoRect &roi, double xRes, double yRes);
    static Line toLine(const ClipperLib::Path &path, const GeoRect &roi, double xRes, double yRes);

private:
    static int inRange(double value, double min, double max, double margin);
    static Line inflateAtChartEdges(const Line &area, Config clipConfig);
    static inline ClipperLib::IntPoint toIntPoint(const Pos &pos,
                                                  const GeoRect &roi,
                                                  double xRes,
                                                  double yRes);
    static inline Pos fromIntPoint(const ClipperLib::IntPoint &intPoint, const GeoRect &clip, double xRes, double yRes);
};
