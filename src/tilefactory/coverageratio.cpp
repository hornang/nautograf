#include "tilefactory/triangulator.h"

#include "coverageratio.h"

CoverageRatio::CoverageRatio(const GeoRect &rect)
    : m_rect(rect)
{
}

float CoverageRatio::coveredArea() const
{
    std::vector<std::vector<Triangulator::Point>> lines;

    for (Clipper2Lib::PathD path : m_coverage) {
        std::vector<Triangulator::Point> line;

        for (Clipper2Lib::PointD point : path) {
            line.push_back({ point.x, point.y });
        }
        lines.push_back(line);
    }

    std::vector<Triangulator::Point> triangles = Triangulator::calc(lines);

    assert((triangles.size() % 3) == 0);

    float area = 0;

    for (int i = 0; i < triangles.size() / 3; i++) {
        const Triangulator::Point &a = triangles[i * 3];
        const Triangulator::Point &b = triangles[i * 3 + 1];
        const Triangulator::Point &c = triangles[i * 3 + 2];

        // https://en.wikipedia.org/wiki/Triangle#Using_coordinates
        float arg = a[0] * (b[1] - c[1])
            + b[0] * (c[1] - a[1])
            + c[0] * (a[1] - b[1]);
        area += 0.5f * fabs(arg);
    }

    return area;
}

float CoverageRatio::ratio() const
{
    return coveredArea() / (m_rect.width() * m_rect.height());
}

void CoverageRatio::accumulate(const capnp::List<ChartData::CoverageArea>::Reader &coverages)
{
    Clipper2Lib::PathsD input = m_coverage;

    for (const ChartData::CoverageArea::Reader &coverage : coverages) {
        for (const ChartData::Polygon::Reader &polygon : coverage.getPolygons()) {
            Clipper2Lib::PathD newCoveragePath;
            for (const ChartData::Position::Reader &pos : polygon.getMain()) {
                newCoveragePath.push_back({ pos.getLatitude(), pos.getLongitude() });
            }
            input.push_back(newCoveragePath);
        }
    }

    m_coverage = Clipper2Lib::Union(input, Clipper2Lib::FillRule::EvenOdd, 5);
}
