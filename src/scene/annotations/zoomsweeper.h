#pragma once

#include <QList>
#include <QRect>
#include <QTransform>

#include "scene/annotations/types.h"

class ZoomSweeper
{

public:
    ZoomSweeper(float maxZoom, const QRect &region);
    void calcSymbols(std::vector<AnnotationSymbol> &symbols);
    void calcLabels(const std::vector<AnnotationSymbol> &symbols,
                    std::vector<AnnotationLabel> &labels);

private:
    static constexpr float m_zoomRatios[] = { 5, 4, 3, 2, 1, 0 };

    struct TestZoom
    {
        QTransform transform;
        float zoom = 0;
    };

    TestZoom m_testZooms[std::size(m_zoomRatios)];
    QRect m_region;
};
