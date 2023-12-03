#pragma once

#include <iostream>

#include "tilefactory_export.h"

class TILEFACTORY_EXPORT Pos
{
public:
    Pos() = default;
    Pos(double lat, double lon);
    bool operator!=(const Pos &pos) const;
    bool operator==(const Pos &pos) const;
    inline double lat() const { return m_lat; }
    inline double lon() const { return m_lon; }
    inline void setLat(double lat) { m_lat = lat; }
    inline void setLon(double lon) { m_lon = lon; }

private:
    /// North/South coordinate in degrees
    double m_lat = 0;

    /// East/West coordinate in degrees
    double m_lon = 0;
};

std::ostream &operator<<(std::ostream &os, const Pos &pos);
