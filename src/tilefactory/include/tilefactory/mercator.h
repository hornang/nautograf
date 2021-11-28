#pragma once

class Mercator
{
public:
    static double mercatorWidth(double leftLongitude,
                                double rightLongitude,
                                double pixelsPerLongitude);

    static double mercatorHeightInverse(double topLatitude,
                                        double height,
                                        double pixelsPerLongitude);

    static double mercatorWidthInverse(double leftLongitude,
                                       double pixels,
                                       double pixelsPerLongitude);

    static double mercatorHeight(double topLatitude,
                                 double bottomLatitude,
                                 double pixelsPerLongitude);
    static double toLatitude(double y);
    static double toLongitude(double x);
};
