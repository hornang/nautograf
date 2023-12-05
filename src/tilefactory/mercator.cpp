#define _USE_MATH_DEFINES
#include <cassert>
#include <cmath>

#include "tilefactory/mercator.h"

static const double latLimit = atan(sinh(M_PI)) * 180. / M_PI;

double Mercator::mercatorNormalizedHeight(double topLat, double bottomLat)
{
    double phiTop = topLat * M_PI / 180.;
    double phiBottom = bottomLat * M_PI / 180.;

    double logArgTop = tan(M_PI / 4 + phiTop / 2);
    double logArgBottom = tan(M_PI / 4 + phiBottom / 2);

    assert(logArgTop > 0);
    assert(logArgBottom > 0);

    double yTop = log(logArgTop);
    double yBottom = log(logArgBottom);

    return yTop - yBottom;
}

double Mercator::mercatorHeight(double topLat, double bottomLat, double pixelsPerLon)
{
    double pixelsPerLonRadians = pixelsPerLon * 180. / M_PI;
    return pixelsPerLonRadians * mercatorNormalizedHeight(topLat, bottomLat);
}

double Mercator::mercatorHeightInverse(const double topLatitude,
                                       const double height,
                                       const double pixelsPerLongitude)
{
    double pixelsPerLongitudeRadians = pixelsPerLongitude * 180. / M_PI;
    double phiTop = topLatitude * M_PI / 180.;
    double logArg = tan(M_PI / 4 + phiTop / 2);
    assert(logArg > 0);
    double yTop = log(logArg);
    double expArg = yTop - height / pixelsPerLongitudeRadians;
    double expResult = exp(expArg);
    double phiBottom = 2 * atan(expResult) - M_PI / 2;
    return phiBottom * 180 / M_PI;
}

double Mercator::toLongitude(double x)
{
    return x * 360. - 180.;
}

double Mercator::toLatitude(double y)
{
    double expArg = M_PI * (1 - 2 * y);
    double expResult = exp(expArg);
    double phiBottom = 2 * (atan(expResult) - M_PI / 4);
    return phiBottom * 180 / M_PI;
}

double Mercator::mercatorNormalizedWidth(double leftLon, double rightLon)
{
    double phiLeft = leftLon * M_PI / 180.;
    double phiRight = rightLon * M_PI / 180.;

    return phiRight - phiLeft;
}

double Mercator::mercatorWidth(double leftLongitude,
                               double rightLongitude,
                               double pixelsPerLongitude)
{
    double pixelsPerLongitudeRadians = pixelsPerLongitude * 180. / M_PI;
    double phiLeft = leftLongitude * M_PI / 180.;
    double phiRight = rightLongitude * M_PI / 180.;

    return pixelsPerLongitudeRadians * mercatorNormalizedWidth(leftLongitude, rightLongitude);
}

double Mercator::mercatorWidthInverse(double leftLongitude,
                                      double pixels,
                                      double pixelsPerLongitude)
{
    return leftLongitude + pixels / pixelsPerLongitude;
}

double Mercator::mercatorWidthInverse(double pixels, double pixelsPerLongitude)
{
    return pixels / pixelsPerLongitude;
}
