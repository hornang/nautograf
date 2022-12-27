#pragma once

#include <clipper2/clipper.h>

#include "tilefactory/chart.h"
#include "tilefactory/georect.h"

class CoverageRatio
{
public:
    CoverageRatio(const GeoRect &rect);
    void accumulate(const capnp::List<ChartData::CoverageArea>::Reader &coverages);
    float ratio() const;

private:
    float coveredArea() const;
    GeoRect m_rect;
    Clipper2Lib::PathsD m_coverage;
};
