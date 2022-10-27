#include <assert.h>

#include "tilefactory/chartclipper.h"
#include "tilefactory/georect.h"
#include "tilefactory/pos.h"

ClipperLib::Path ChartClipper::toClipperPath(const capnp::List<ChartData::Position>::Reader &positions,
                                             const GeoRect &roi, double xRes, double yRes)
{
    ClipperLib::Path path;
    ClipperLib::IntPoint prevPoint;

    for (const auto &p : positions) {
        Pos pos(p.getLatitude(), p.getLongitude());
        ClipperLib::IntPoint point = toIntPoint(pos, roi, xRes, yRes);
        if (point == prevPoint) {
            continue;
        }
        path.push_back(point);
    }

    return path;
}

ChartClipper::Line ChartClipper::toLine(const ClipperLib::Path &path,
                                        const GeoRect &roi, double xRes, double yRes)
{
    ChartClipper::Line polygon;

    for (const ClipperLib::IntPoint &intPoint : path) {
        Pos p = fromIntPoint(intPoint, roi, xRes, yRes);
        polygon.push_back(Pos(p.lat(), p.lon()));
    }

    return polygon;
}

std::vector<ChartClipper::Polygon> ChartClipper::clipPolygon(const ChartData::Polygon::Reader &polygon,
                                                             Config clipConfig)
{
    assert(clipConfig.longitudeResolution > 0);
    assert(clipConfig.latitudeResolution > 0);

    const GeoRect &boundingBox = clipConfig.box;
    GeoRect clipRect(boundingBox.top() + clipConfig.latitudeMargin,
                     boundingBox.bottom() - clipConfig.latitudeMargin,
                     boundingBox.left() - clipConfig.longitudeMargin,
                     boundingBox.right() + clipConfig.longitudeMargin);

    double xRes = (clipConfig.box.right() - clipConfig.box.left()) / clipConfig.longitudeResolution;
    double yRes = (clipConfig.box.top() - clipConfig.box.bottom()) / clipConfig.latitudeResolution;

    // Add a fudge factor here to increase resolution
    xRes *= 2;
    yRes *= 2;

    ClipperLib::Paths paths;

    GeoRect geoRect(clipRect.top(), clipRect.bottom(), clipRect.left(), clipRect.right());

    paths.push_back(toClipperPath(polygon.getMain(), clipRect, xRes, yRes));

    ClipperLib::Path clipPath;
    clipPath.push_back(ClipperLib::IntPoint(0, 0));
    clipPath.push_back(ClipperLib::IntPoint(0, yRes));
    clipPath.push_back(ClipperLib::IntPoint(xRes, yRes));
    clipPath.push_back(ClipperLib::IntPoint(xRes, 0));

    ClipperLib::Paths clipPaths;
    clipPaths.push_back(clipPath);
    ClipperLib::Clipper mainClipper;
    mainClipper.AddPaths(paths, ClipperLib::ptSubject, true);
    mainClipper.AddPaths(clipPaths, ClipperLib::ptClip, true);

    ClipperLib::Paths solution;
    mainClipper.Execute(ClipperLib::ctIntersection,
                        solution,
                        ClipperLib::pftEvenOdd,
                        ClipperLib::pftEvenOdd);
    std::vector<Polygon> output;

    if (solution.empty()) {
        return {};
    }

    ClipperLib::Paths holePaths;
    for (const capnp::List<ChartData::Position>::Reader &hole : polygon.getHoles()) {
        holePaths.push_back(toClipperPath(hole, clipRect, xRes, yRes));
    }

    for (const ClipperLib::Path &mainAreas : solution) {
        Polygon area;
        auto polygon = toLine(mainAreas, geoRect, xRes, yRes);

        if (clipConfig.moveOutEdges) {
            polygon = inflateAtChartEdges(polygon, clipConfig);
        }

        area.main = polygon;

        if (holePaths.empty()) {
            output.push_back(area);
            continue;
        }

        ClipperLib::Paths other = holePaths;
        ClipperLib::Clipper holeClipper;
        holeClipper.AddPaths(other, ClipperLib::ptSubject, true);
        holeClipper.AddPaths({ mainAreas }, ClipperLib::ptClip, true);

        ClipperLib::Paths holeResults;
        holeClipper.Execute(ClipperLib::ctIntersection,
                            holeResults,
                            ClipperLib::pftEvenOdd,
                            ClipperLib::pftEvenOdd);
        for (const ClipperLib::Path &path : holeResults) {
            area.holes.push_back(toLine(path, geoRect, xRes, yRes));
        }
        output.push_back(area);
    }
    return output;
}

inline ClipperLib::IntPoint ChartClipper::toIntPoint(const Pos &pos, const GeoRect &roi, double xRes, double yRes)
{
    int x = (pos.lon() - roi.left()) / (roi.right() - roi.left()) * xRes;
    int y = (pos.lat() - roi.bottom()) / (roi.top() - roi.bottom()) * yRes;
    return ClipperLib::IntPoint(x, y);
}

inline Pos ChartClipper::fromIntPoint(const ClipperLib::IntPoint &intPoint, const GeoRect &roi, double xRes, double yRes)
{
    double lon = static_cast<double>(intPoint.X) / xRes * (roi.right() - roi.left()) + roi.left();
    double lat = static_cast<double>(intPoint.Y) / yRes * (roi.top() - roi.bottom()) + roi.bottom();
    return Pos(lat, lon);
}

ChartClipper::Line ChartClipper::inflateAtChartEdges(const Line &line, Config clipConfig)
{
    Pos previousPosition = line.back();
    int longitudeState = inRange(previousPosition.lon(),
                                 clipConfig.chartBoundingBox.left(),
                                 clipConfig.chartBoundingBox.right(),
                                 clipConfig.longitudeResolution);
    int latitudeState = inRange(previousPosition.lat(),
                                clipConfig.chartBoundingBox.bottom(),
                                clipConfig.chartBoundingBox.top(),
                                clipConfig.latitudeResolution);
    bool outside = (longitudeState != 0) || (latitudeState != 0);

    Line output;

    for (Pos position : line) {
        Pos newPosition = position;

        int newLongitudeState = inRange(position.lon(),
                                        clipConfig.chartBoundingBox.left(),
                                        clipConfig.chartBoundingBox.right(),
                                        clipConfig.longitudeResolution);

        int newLatitudeState = inRange(position.lat(),
                                       clipConfig.chartBoundingBox.bottom(),
                                       clipConfig.chartBoundingBox.top(),
                                       clipConfig.latitudeResolution);

        bool newOutside = (newLongitudeState != 0) || (newLatitudeState != 0);

        if (newOutside && !outside) {
            output.push_back(position);
        }

        if (newLongitudeState == 1) {
            newPosition.setLon(clipConfig.chartBoundingBox.right() + 2 * clipConfig.longitudeMargin);
        } else if (newLongitudeState == -1) {
            newPosition.setLon(clipConfig.chartBoundingBox.left() - 2 * clipConfig.longitudeMargin);
        }

        if (newLatitudeState == 1) {
            newPosition.setLat(clipConfig.chartBoundingBox.top() + 2 * clipConfig.latitudeMargin);
        } else if (newLatitudeState == -1) {
            newPosition.setLat(clipConfig.chartBoundingBox.bottom() - 2 * clipConfig.latitudeMargin);
        }

        if (!newOutside && outside) {
            output.push_back(previousPosition);
        }
        outside = newOutside;

        output.push_back(newPosition);
        previousPosition = position;
    }
    return output;
}

int ChartClipper::inRange(double value, double min, double max, double margin)
{
    if (value <= min + margin) {
        return -1;
    } else if (value >= max - margin) {
        return 1;
    } else {
        return 0;
    }
}
