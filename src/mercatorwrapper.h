#pragma once

#include <QObject>

#include "tilefactory/mercator.h"

class MercatorWrapper : public QObject
{
    Q_OBJECT

public slots:
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
