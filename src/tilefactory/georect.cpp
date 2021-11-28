#include <algorithm>

#include "tilefactory/georect.h"

GeoRect::GeoRect(double top, double bottom, double left, double right)
    : m_top(top)
    , m_bottom(bottom)
    , m_left(left)
    , m_right(right)
{
}

bool GeoRect::operator==(const GeoRect &other)
{
    return m_left == other.m_left && m_right == other.m_right && m_top == other.m_top && m_bottom == other.m_bottom;
}

bool GeoRect::contains(double lat, double lon) const
{
    return lon >= m_left && lon <= m_right && lat < m_top && lat > m_bottom;
}

bool GeoRect::intersects(const GeoRect &other) const
{
    if (m_left > other.m_right || m_right < other.m_left || m_bottom > other.m_top || m_top < other.m_bottom) {
        return false;
    }

    return true;
}

GeoRect GeoRect::intersection(const GeoRect &other) const
{
    if (!intersects(other)) {
        return GeoRect();
    }

    GeoRect output = *this;

    output.m_top = std::min(output.m_top, other.m_top);
    output.m_bottom = std::max(output.m_bottom, other.m_bottom);
    output.m_left = std::max(output.m_left, other.m_left);
    output.m_right = std::min(output.m_right, other.m_right);

    return output;
}

bool GeoRect::encloses(const GeoRect &other) const
{
    return (m_left <= other.m_left
            && m_right >= other.m_right
            && m_top >= other.m_top
            && m_bottom <= other.m_bottom);
}

std::ostream &operator<<(std::ostream &os, const GeoRect &rect)
{
    os << "top: " << rect.top() << " bottom: " << rect.bottom() << " left: " << rect.left() << " right: " << rect.right();
    return os;
}

bool operator==(const GeoRect &a, const GeoRect &b)
{
    return a.left() == b.left()
        && a.right() == b.right()
        && a.top() == b.top()
        && a.bottom() == b.bottom();
}
