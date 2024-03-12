#pragma once

#include <QList>
#include <QTransform>

#include "scene/annotations/types.h"

class ZoomSweeper
{

public:
    ZoomSweeper(float maxZoom);
    void calcSymbols(QList<AnnotationSymbol> &annotations);
    QList<AnnotationLabel> calcLabels(const QList<AnnotationSymbol> &annotations);

private:
    static constexpr float m_zoomRatios[] = { 5, 4, 3, 2, 1, 0 };
    QTransform m_transforms[std::size(m_zoomRatios)];
};
