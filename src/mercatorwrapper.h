#pragma once

#include <QObject>

#include "tilefactory/mercator.h"

class MercatorWrapper : public QObject
{
    Q_OBJECT

public slots:
    double mercatorWidth(double leftLon, double rightLon, double pixelsPerLon)
    {
        return Mercator::mercatorWidth(leftLon, rightLon, pixelsPerLon);
    }

    double mercatorHeight(double topLat, double bottomLat, double pixelsPerLon)
    {
        return Mercator::mercatorHeight(topLat, bottomLat, pixelsPerLon);
    }

    double mercatorWidthInverse(double leftLon, double pixels, double pixelsPerLon)
    {
        return Mercator::mercatorWidthInverse(leftLon, pixels, pixelsPerLon);
    }

    double mercatorHeightInverse(double topLat, double pixels, double pixelsPerLon)
    {
        return Mercator::mercatorHeightInverse(topLat, pixels, pixelsPerLon);
    }

    double pixelsPerLonInterval(double leftLon, double rightLon, double pixels)
    {
        double pixelsPerLongitudeRadians = pixels / Mercator::mercatorNormalizedWidth(leftLon, rightLon);
        return pixelsPerLongitudeRadians * M_PI / 180.;
    }

    double pixelsPerLatInterval(const double topLat, const double bottomLat, const double height)
    {
        double pixelsPerLongitudeRadians = height / Mercator::mercatorNormalizedHeight(topLat, bottomLat);
        return pixelsPerLongitudeRadians * M_PI / 180.0;
    }

    QPointF offsetPosition(QPointF startPosition, qreal pixelsPerLongitude, QPointF mouseOffset)
    {
        double lat = Mercator::mercatorHeightInverse(startPosition.y(),
                                                     mouseOffset.y(),
                                                     pixelsPerLongitude);
        double lon = Mercator::mercatorWidthInverse(startPosition.x(),
                                                    mouseOffset.x(),
                                                    pixelsPerLongitude);
        return QPointF(lon, lat);
    }
};
