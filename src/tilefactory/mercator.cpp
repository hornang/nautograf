#define _USE_MATH_DEFINES
#include <cmath>

#include "tilefactory/mercator.h"

static const double latLimit = atan(sinh(M_PI)) * 180. / M_PI;

double Mercator::mercatorHeightInverse(const double topLatitude,
                                       const double height,
                                       const double pixelsPerLongitude)
{
    double pixelsPerLongitudeRadians = pixelsPerLongitude * 180. / M_PI;
    double phiTop = topLatitude * M_PI / 180.;
    double yTop = log(tan(M_PI / 4 + phiTop / 2));
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

double Mercator::mercatorWidth(double leftLongitude,
                               double rightLongitude,
                               double pixelsPerLongitude)
{

    double pixelsPerLongitudeRadians = pixelsPerLongitude * 180. / M_PI;
    double phiLeft = leftLongitude * M_PI / 180.;
    double phiRight = rightLongitude * M_PI / 180.;

    double width = pixelsPerLongitudeRadians * (phiRight - phiLeft);
    return width;
}

double Mercator::mercatorWidthInverse(double leftLongitude,
                                      double pixels,
                                      double pixelsPerLongitude)
{
    return leftLongitude + pixels / pixelsPerLongitude;
}

double Mercator::mercatorHeight(double topLatitude,
                                double bottomLatitude,
                                double pixelsPerLongitude)
{
    double pixelsPerLongitudeRadians = pixelsPerLongitude * 180. / M_PI;
    double phiTop = topLatitude * M_PI / 180.;
    double phiBottom = bottomLatitude * M_PI / 180.;

    double yTop = (log(tan(M_PI / 4 + phiTop / 2)));
    double yBottom = (log(tan(M_PI / 4 + phiBottom / 2)));

    double height = pixelsPerLongitudeRadians * (yTop - yBottom);
    return height;
}
