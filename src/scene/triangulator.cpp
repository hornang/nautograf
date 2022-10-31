#include <QList>

#include <mapbox/earcut.hpp>

#include "triangulator.h"
#include <array>
#include <iostream>

Triangulator::Point Triangulator::getPoint(const std::vector<std::vector<Point>> &polygon, int index)
{
    for (const std::vector<Point> &points : polygon) {
        if (index >= points.size()) {
            index -= points.size();
            continue;
        }
        return { points[index][0], points[index][1] };
    }

    return {};
}

std::vector<Triangulator::Point> Triangulator::calc(const std::vector<std::vector<Point>> &polygon)
{
    // From https://github.com/mapbox/earcut.hpp

    // The number type to use for tessellation
    using Coord = double;

    // The index type. Defaults to uint32_t, but you can also pass uint16_t if you know that your
    // data won't have more than 65536 vertices.
    using N = uint32_t;

    // Run tessellation
    // Returns array of indices that refer to the vertices of the input polygon.
    // e.g: the index 6 would refer to {25, 75} in this example.
    // Three subsequent indices form a triangle. Output triangles are clockwise.
    std::vector<N> indices = mapbox::earcut<N>(polygon);

    std::vector<Point> result;
    result.resize(indices.size());

    int counter = 0;
    for (const auto &index : indices) {
        result[counter] = getPoint(polygon, index);
        counter++;
    }

    return result;
}
