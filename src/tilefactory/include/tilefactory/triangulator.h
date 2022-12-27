#pragma once

#include <array>
#include <vector>

class Triangulator
{
public:
    Triangulator() = delete;
    using Point = std::array<double, 2>;
    static std::vector<Triangulator::Point> calc(const std::vector<std::vector<Point>> &polygons);

private:
    static Point getPoint(const std::vector<std::vector<Point>> &polygon, int index);
};
