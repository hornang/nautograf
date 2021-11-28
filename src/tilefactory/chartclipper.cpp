#include <assert.h>

#include "tilefactory/chartclipper.h"
#include "tilefactory/georect.h"
#include "tilefactory/pos.h"

std::vector<ChartClipper::Polygon> ChartClipper::clipPolygons(const capnp::List<ChartData::Polygon>::Reader &polygons,
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

    for (const auto &polygon : polygons) {
        ClipperLib::Path path;
        ClipperLib::IntPoint prevPoint;
        for (const auto &p : polygon.getPositions()) {
            Pos pos(p.getLatitude(), p.getLongitude());
            ClipperLib::IntPoint point = toIntPoint(pos, geoRect, xRes, yRes);
            if (point == prevPoint) {
                continue;
            }
            path.push_back(point);
        }
        paths.push_back(path);
    }

    ClipperLib::Path clipPath;
    clipPath.push_back(ClipperLib::IntPoint(0, 0));
    clipPath.push_back(ClipperLib::IntPoint(0, yRes));
    clipPath.push_back(ClipperLib::IntPoint(xRes, yRes));
    clipPath.push_back(ClipperLib::IntPoint(xRes, 0));

    ClipperLib::Paths clipPaths;
    clipPaths.push_back(clipPath);
    ClipperLib::Clipper clipper;
    clipper.AddPaths(paths, ClipperLib::ptSubject, true);
    clipper.AddPaths(clipPaths, ClipperLib::ptClip, true);

    ClipperLib::Paths solution;
    clipper.Execute(ClipperLib::ctIntersection,
                    solution,
                    ClipperLib::pftEvenOdd,
                    ClipperLib::pftEvenOdd);
    std::vector<Polygon> output;

    for (const ClipperLib::Path &path : solution) {
        Polygon polygon;
        for (const ClipperLib::IntPoint &intPoint : path) {
            Pos p = fromIntPoint(intPoint, geoRect, xRes, yRes);
            polygon.push_back(Pos(p.lat(), p.lon()));
        }
        output.push_back(polygon);
    }

    if (clipConfig.moveOutEdges) {
        output = moveOutChartEdges(output, clipConfig);
    }
    return output;
}

template <typename T>
std::vector<T> ChartClipper::clipByPosition(const std::vector<T> &input, const GeoRect &rect)
{
    std::vector<T> clipped;
    for (const T &item : input) {
        if (rect.contains(item.position)) {
            clipped.push_back(item);
        }
    }
    return clipped;
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

std::vector<ChartClipper::Polygon> ChartClipper::moveOutChartEdges(const std::vector<Polygon> &polygons,
                                                                   Config clipConfig)
{
    std::vector<Polygon> output;

    for (const Polygon &polygon : polygons) {
        if (polygon.empty()) {
            continue;
        }

        Pos previousPosition = polygon.back();
        int longitudeState = inRange(previousPosition.lon(),
                                     clipConfig.chartBoundingBox.left(),
                                     clipConfig.chartBoundingBox.right(),
                                     clipConfig.longitudeResolution);
        int latitudeState = inRange(previousPosition.lat(),
                                    clipConfig.chartBoundingBox.bottom(),
                                    clipConfig.chartBoundingBox.top(),
                                    clipConfig.latitudeResolution);
        bool outside = (longitudeState != 0) || (latitudeState != 0);

        Polygon polygonOutput;

        for (Pos position : polygon) {
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
                polygonOutput.push_back(position);
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
                polygonOutput.push_back(previousPosition);
            }
            outside = newOutside;

            polygonOutput.push_back(newPosition);
            previousPosition = position;
        }
        output.push_back(polygonOutput);
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
