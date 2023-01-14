#pragma once

#include <iostream>

#include "pos.h"

#include "tilefactory_export.h"

class TILEFACTORY_EXPORT GeoRect
{
public:
    GeoRect() = default;
    GeoRect(double top, double bottom, double left, double right);
    GeoRect(const GeoRect &other) = default;
    GeoRect &operator=(const GeoRect &other) = default;
    bool operator==(const GeoRect &other);
    bool contains(double lat, double lon) const;
    Pos topLeft() const { return Pos(m_top, m_left); }
    Pos topRight() const { return Pos(m_top, m_right); }
    Pos bottomLeft() const { return Pos(m_bottom, m_left); }
    Pos bottomRight() const { return Pos(m_bottom, m_right); }
    double lonSpan() const { return m_right - m_left; }
    double latSpan() const { return m_top - m_bottom; }
    double top() const { return m_top; }
    double bottom() const { return m_bottom; }
    double right() const { return m_right; }
    double left() const { return m_left; }
    double width() const { return m_right - m_left; }
    double height() const { return m_top - m_bottom; }
    bool isNull() const { return m_top == 0 && m_bottom == 0 && m_left == 0 && m_right == 0; }
    /*!
     *  Returns true of other rectangle is totally inside this Rectangle
     */
    bool encloses(const GeoRect &other) const;
    bool intersects(const GeoRect &rect) const;
    GeoRect intersection(const GeoRect &rect) const;

private:
    double m_top = 0;
    double m_bottom = 0;
    double m_left = 0;
    double m_right = 0;
};

bool operator==(const GeoRect &a, const GeoRect &b);
std::ostream &operator<<(std::ostream &os, const GeoRect &rect);
