#pragma once

#include <optional>
#include <string>

#include "chart.h"
#include "georect.h"

class ITileSource
{
public:
    virtual std::shared_ptr<Chart> create(const GeoRect &boundingBox,
                                              int pixelsPerLongitude) = 0;
    virtual GeoRect extent() const = 0;
    virtual int scale() const = 0;
};
