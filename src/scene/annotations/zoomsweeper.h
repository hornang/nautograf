#pragma once

#include <QList>
#include <QTransform>

#include "scene/annotations/types.h"

class ZoomSweeper
{

public:
    ZoomSweeper(float maxZoom);
    void calcSymbols(std::vector<AnnotationSymbol> &annotations);
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
};
