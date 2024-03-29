#pragma once

#include "tilefactory_export.h"

class TILEFACTORY_EXPORT Mercator
{
public:
    static double mercatorWidth(double leftLongitude,
                                double rightLongitude,
                                double pixelsPerLongitude);

    static double mercatorNormalizedHeight(double topLat, double bottomLat);

    static double mercatorHeightInverse(double topLatitude,
                                        double height,
                                        double pixelsPerLongitude);

    static double mercatorNormalizedWidth(double leftLon, double rightLon);

    static double mercatorWidthInverse(double leftLongitude,
                                       double pixels,
                                       double pixelsPerLongitude);

    static double mercatorWidthInverse(double pixels, double pixelsPerLon);

    static double mercatorHeight(double topLatitude,
                                 double bottomLatitude,
                                 double pixelsPerLongitude);
    static double toLatitude(double y);
    static double toLongitude(double x);
};
