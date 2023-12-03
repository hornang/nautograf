#include "tilefactory/pos.h"

Pos::Pos(double lat, double lon)
    : m_lat(lat)
    , m_lon(lon)
{
}

bool Pos::operator!=(const Pos &pos) const
{
    return (m_lat != pos.lat() || m_lon != pos.lon());
}

bool Pos::operator==(const Pos &pos) const
{
    return (m_lat == pos.lat() && m_lon == pos.lon());
}

std::ostream &operator<<(std::ostream &os, const Pos &pos)
{
    os << pos.lat() << " " << pos.lon();
    return os;
}
