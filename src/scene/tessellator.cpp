#include <QLocale>

#include <limits>

#include "annotations/annotater.h"
#include "annotations/zoomsweeper.h"
#include "cutlines/cutlines.h"
#include "tessellator.h"
#include "tilefactory/mercator.h"
#include "tilefactory/triangulator.h"

static const QColor labelColor = Qt::black;
static const QColor symbolLabelColor = Qt::black;
static const QColor criticalWaterColor(165, 202, 159);
static const QColor landAreaColor(255, 240, 190);
static const QColor builtUpAreaColor(240, 220, 160);
static const QColor depthContourColor(110, 190, 230);
static const QColor coastLineColor = landAreaColor.darker(200);
static const QColor builtUpAreaColorBorder = builtUpAreaColor.darker(150);
static const QColor pontoonColor = builtUpAreaColor.darker(120);
static const QColor pontoonBorderColor = pontoonColor.darker(150);
static const QColor roadColor = landAreaColor.darker(120);
static const QColor roadColorBorder = roadColor.darker(150);
static const QColor tilePlaceholderOutlineColor = QColor(200, 200, 200);
static const QColor tilePlaceholderFillColor = QColor(240, 240, 240);

// Arbitrary chosen conversion factor to transform lat/lon to an internal mercator
// projection that can be linearly transformed in scene graph/vertex shader.
const int s_pixelsPerLon = 1000;

using namespace std;

namespace {
QPointF posToMercator(const ChartData::Position::Reader &pos)
{
    return { Mercator::mercatorWidth(0, pos.getLongitude(), s_pixelsPerLon),
             Mercator::mercatorHeight(0, pos.getLatitude(), s_pixelsPerLon) };
}

QPointF posToMercator(double lat, double lon)
{
    return { Mercator::mercatorWidth(0, lon, s_pixelsPerLon),
             Mercator::mercatorHeight(0, lat, s_pixelsPerLon) };
}

QPointF posToMercator(const Pos &pos)
{
    return { Mercator::mercatorWidth(0, pos.lon(), s_pixelsPerLon),
             Mercator::mercatorHeight(0, pos.lat(), s_pixelsPerLon) };
}

template <typename T>
QList<PolygonNode::Vertex> drawPolygons(const typename capnp::List<T>::Reader &areas,
                                        float z,
                                        std::function<QColor(const typename T::Reader &)> colorFunc)
{
    QList<PolygonNode::Vertex> vertices;
    int vertexCount = 0;

    for (const auto &area : areas) {
        QColor color = colorFunc(area);

        if (!color.isValid()) {
            continue;
        }

        for (const ChartData::Polygon::Reader &polygon : area.getPolygons()) {
            std::vector<std::vector<Triangulator::Point>> polylines;
            std::vector<Triangulator::Point> polyline;

            capnp::List<ChartData::Position>::Reader main = polygon.getMain();
            for (ChartData::Position::Reader pos : main) {
                QPointF p = posToMercator(pos);
                polyline.push_back({ p.x(), p.y() });
            }
            polylines.push_back(polyline);

            for (const auto &hole : polygon.getHoles()) {
                std::vector<Triangulator::Point> polyline;

                for (const auto &pos : hole) {
                    QPointF p = posToMercator(pos);
                    polyline.push_back({ p.x(), p.y() });
                }
                polylines.push_back(polyline);
            }

            std::vector<Triangulator::Point> triangles = Triangulator::calc(polylines);
            vertices.resize(vertices.size() + triangles.size());

            for (const auto &point : triangles) {
                vertices[vertexCount] = { static_cast<float>(point[0]),
                                          static_cast<float>(point[1]),
                                          z,
                                          static_cast<uchar>(color.red()),
                                          static_cast<uchar>(color.green()),
                                          static_cast<uchar>(color.blue()),
                                          static_cast<uchar>(color.alpha()) };
                vertexCount++;
            }
        }
    }

    return vertices;
}

QList<LineNode::Vertex> tessellateLine(const QList<QPointF> &points,
                                       const QColor &color)
{
    Q_ASSERT(points.size() > 1);
    QList<LineNode::Vertex> output;
    const float z = 0.5;

    output.resize((points.size() - 1) * 6);

    for (int i = 1; i < points.size(); i++) {
        const QPointF &prevPoint = points.at(i - 1);
        const QPointF &point = points.at(i);
        QVector2D forward(point - prevPoint);
        QVector2D rightNormal(forward.y(), -forward.x());
        rightNormal.normalize();

        int vertexNo = (i - 1) * 6;
        LineNode::Vertex *data = output.data() + vertexNo;

        data[0] = { static_cast<float>(prevPoint.x()),
                    static_cast<float>(prevPoint.y()),
                    z,
                    static_cast<float>(-rightNormal.x()),
                    static_cast<float>(-rightNormal.y()),
                    static_cast<uchar>(color.red()),
                    static_cast<uchar>(color.green()),
                    static_cast<uchar>(color.blue()),
                    255 };

        data[1] = { static_cast<float>(point.x()),
                    static_cast<float>(point.y()),
                    z,
                    static_cast<float>(rightNormal.x()),
                    static_cast<float>(rightNormal.y()),
                    static_cast<uchar>(color.red()),
                    static_cast<uchar>(color.green()),
                    static_cast<uchar>(color.blue()),
                    255 };

        data[2] = { static_cast<float>(prevPoint.x()),
                    static_cast<float>(prevPoint.y()),
                    z,
                    static_cast<float>(rightNormal.x()),
                    static_cast<float>(rightNormal.y()),
                    static_cast<uchar>(color.red()),
                    static_cast<uchar>(color.green()),
                    static_cast<uchar>(color.blue()),
                    255 };

        data[3] = { static_cast<float>(prevPoint.x()),
                    static_cast<float>(prevPoint.y()),
                    z,
                    static_cast<float>(-rightNormal.x()),
                    static_cast<float>(-rightNormal.y()),
                    static_cast<uchar>(color.red()),
                    static_cast<uchar>(color.green()),
                    static_cast<uchar>(color.blue()),
                    255 };

        data[4] = { static_cast<float>(point.x()),
                    static_cast<float>(point.y()),
                    z,
                    static_cast<float>(rightNormal.x()),
                    static_cast<float>(rightNormal.y()),
                    static_cast<uchar>(color.red()),
                    static_cast<uchar>(color.green()),
                    static_cast<uchar>(color.blue()),
                    255 };

        data[5] = { static_cast<float>(point.x()),
                    static_cast<float>(point.y()),
                    z,
                    static_cast<float>(-rightNormal.x()),
                    static_cast<float>(-rightNormal.y()),
                    static_cast<uchar>(color.red()),
                    static_cast<uchar>(color.green()),
                    static_cast<uchar>(color.blue()),
                    255 };
    }

    return output;
}

template <typename T>
QList<LineNode::Vertex> drawLines(const typename capnp::List<T>::Reader &areas,
                                  std::function<QColor(const typename T::Reader &)> colorFunc)
{
    QList<LineNode::Vertex> vertices;
    int vertexCount = 0;

    for (const auto &area : areas) {
        QColor color = colorFunc(area);

        if (!color.isValid()) {
            continue;
        }

        for (const ChartData::Line::Reader &line : area.getLines()) {
            if (line.getPositions().size() < 2) {
                continue;
            }

            QList<QPointF> points;
            points.reserve(line.getPositions().size());

            for (const ChartData::Position::Reader &position : line.getPositions()) {
                points.append(posToMercator(position.getLatitude(), position.getLongitude()));
            }

            vertices.append(tessellateLine(points, color));
        }
    }

    return vertices;
}

/*!
    A demonstrative "stroke polygons" implementation.

    This function strokes polygon border, but has the flaws described below.

    The current scene graph approach has no graphical tile clipping. Clipped
    polygons may extend out of the tile (the clipping margin). The problem is
    that the polygon should not be stroked outside of the tile.

    To solve this the polygon border is interpreted as a line and clipped using
    the cutlines library (Cohen-Sutherland algorithm). For this to work the line
    clipping window must be smaller than the polygon clipping window. Polygon
    clipping happens in the tile source whereas line clipping is done here.

    The difference between the polygon and line clipping window presents a new
    problem: The graphical stacking will place one tiles' graphical items on top
    of graphical items in another tile. This means that a polygon from one tile
    may occlude strokes (of the same polygon) in another tile because the
    line is partly inside the polygon. The result is that polygons crossing tiles
    will have a small section without stroked edges.

    The best way to solve this without introducing graphical tile clipping would
    be to stack graphical items in the same order as their source chart and not
    group them into tiles first which are then stacked.
*/
template <typename T>
QList<LineNode::Vertex> strokePolygons(const typename capnp::List<T>::Reader &areas,
                                       std::function<QColor(const typename T::Reader &)> colorFunc,
                                       const GeoRect &tileRect)
{
    QList<LineNode::Vertex> vertices;
    int vertexCount = 0;

    for (const auto &area : areas) {
        QColor color = colorFunc(area);

        if (!color.isValid()) {
            continue;
        }

        for (const ChartData::Polygon::Reader &polygonHole : area.getPolygons()) {
            std::vector<capnp::List<ChartData::Position>::Reader> pathsToStroke;
            pathsToStroke.push_back(polygonHole.getMain());
            for (capnp::List<ChartData::Position>::Reader holes : polygonHole.getHoles()) {
                pathsToStroke.push_back(holes);
            }

            cutlines::Rect rect { tileRect.left(),
                                  tileRect.right(),
                                  tileRect.bottom(),
                                  tileRect.top() };

            std::vector<cutlines::Line> clippedLines;

            for (capnp::List<ChartData::Position>::Reader &srcLine : pathsToStroke) {
                Q_ASSERT(srcLine.size() > 0);

                cutlines::Line line;

                for (int i = 0; i < srcLine.size(); i++) {
                    line.push_back({ srcLine[i].getLongitude(), srcLine[i].getLatitude() });
                }
                line.push_back({ srcLine[0].getLongitude(),
                                 srcLine[0].getLatitude() });

                std::vector<cutlines::Line> lines = cutlines::clip(line, rect);
                clippedLines.insert(clippedLines.begin(), lines.begin(), lines.end());
            }

            for (const cutlines::Line &line : clippedLines) {
                QList<QPointF> points;
                points.reserve(line.size());

                for (const cutlines::Point &point : line) {
                    points.append(posToMercator(point[1], point[0]));
                }
                vertices.append(tessellateLine(points, color));
            }
        }
    }

    return vertices;
}

QList<AnnotationNode::Vertex> getSymbolVertices(const vector<AnnotationSymbol> &annotations)
{
    int validSymbols = 0;

    for (const AnnotationSymbol &annotation : annotations) {
        if (annotation.symbol.has_value()) {
            validSymbols++;
        }
    }

    QList<AnnotationNode::Vertex> vertices(validSymbols * 6);
    AnnotationNode::Vertex *data = vertices.data();

    for (const auto &annotation : annotations) {
        if (!annotation.symbol.has_value()) {
            continue;
        }

        const auto &symbol = annotation.symbol.value();
        const auto &center = symbol.center;
        const auto &pos = annotation.pos;
        const auto &sourceRect = symbol.coords;
        const auto &symbolSize = symbol.size;
        const auto &color = symbol.color;

        float minZoom = 1000;

        if (annotation.minZoom) {
            minZoom = annotation.minZoom.value();
        }

        data[0] = { static_cast<float>(pos.x()),
                    static_cast<float>(pos.y()),
                    static_cast<float>(-center.x()),
                    static_cast<float>(-center.y()),
                    static_cast<float>(sourceRect.left()),
                    static_cast<float>(sourceRect.top()),
                    static_cast<uchar>(color.red()),
                    static_cast<uchar>(color.green()),
                    static_cast<uchar>(color.blue()),
                    static_cast<uchar>(color.alpha()),
                    minZoom };

        data[1] = { static_cast<float>(pos.x()),
                    static_cast<float>(pos.y()),
                    static_cast<float>(-center.x() + symbolSize.width()),
                    static_cast<float>(-center.y()),
                    static_cast<float>(sourceRect.right()),
                    static_cast<float>(sourceRect.top()),
                    static_cast<uchar>(color.red()),
                    static_cast<uchar>(color.green()),
                    static_cast<uchar>(color.blue()),
                    static_cast<uchar>(color.alpha()),
                    minZoom };

        data[2] = { static_cast<float>(pos.x()),
                    static_cast<float>(pos.y()),
                    static_cast<float>(-center.x()),
                    static_cast<float>(-center.y() + symbolSize.height()),
                    static_cast<float>(sourceRect.left()),
                    static_cast<float>(sourceRect.bottom()),
                    static_cast<uchar>(color.red()),
                    static_cast<uchar>(color.green()),
                    static_cast<uchar>(color.blue()),
                    static_cast<uchar>(color.alpha()),
                    minZoom };

        data[3] = { static_cast<float>(pos.x()),
                    static_cast<float>(pos.y()),
                    static_cast<float>(-center.x()),
                    static_cast<float>(-center.y() + symbolSize.height()),
                    static_cast<float>(sourceRect.left()),
                    static_cast<float>(sourceRect.bottom()),
                    static_cast<uchar>(color.red()),
                    static_cast<uchar>(color.green()),
                    static_cast<uchar>(color.blue()),
                    static_cast<uchar>(color.alpha()),
                    minZoom };

        data[4] = { static_cast<float>(pos.x()),
                    static_cast<float>(pos.y()),
                    static_cast<float>(-center.x() + symbolSize.width()),
                    static_cast<float>(-center.y()),
                    static_cast<float>(sourceRect.right()),
                    static_cast<float>(sourceRect.top()),
                    static_cast<uchar>(color.red()),
                    static_cast<uchar>(color.green()),
                    static_cast<uchar>(color.blue()),
                    static_cast<uchar>(color.alpha()),
                    minZoom };

        data[5] = { static_cast<float>(pos.x()),
                    static_cast<float>(pos.y()),
                    static_cast<float>(-center.x() + symbolSize.width()),
                    static_cast<float>(-center.y() + symbolSize.height()),
                    static_cast<float>(sourceRect.right()),
                    static_cast<float>(sourceRect.bottom()),
                    static_cast<uchar>(color.red()),
                    static_cast<uchar>(color.green()),
                    static_cast<uchar>(color.blue()),
                    static_cast<uchar>(color.alpha()),
                    minZoom };

        data += 6;
    }

    return vertices;
}

QList<AnnotationNode::Vertex> getTextVertices(const vector<AnnotationLabel> &annotationLabels,
                                              const FontImage *fontImage)
{
    QList<AnnotationNode::Vertex> list;
    int glyphCounter = 0;
    QSize imageSize = fontImage->atlasSize();

    for (const auto &annotationLabel : annotationLabels) {
        if (!annotationLabel.minZoom.has_value()) {
            continue;
        }

        float minZoom = annotationLabel.minZoom.value();

        auto glyphs = fontImage->glyphs(annotationLabel.label.text,
                                        annotationLabel.label.pointSize,
                                        annotationLabel.label.font);
        const QColor &color = annotationLabel.label.color;

        for (const msdf_atlas_read::GlyphMapping mapping : glyphs) {
            const QPointF metricsOffset(0, -annotationLabel.boundingBox.top());
            QPointF glyphPos(mapping.targetBounds.left, mapping.targetBounds.top);
            glyphPos += annotationLabel.offset + metricsOffset;
            msdf_atlas_read::Bounds texture = mapping.atlasBounds.getNormalized(imageSize.width(), imageSize.height());
            const auto glyphWidth = static_cast<float>(mapping.targetBounds.getWidth());
            const auto glyphHeight = static_cast<float>(mapping.targetBounds.getHeight());
            const auto &pos = annotationLabel.pos;

            list.resize(list.size() + 6);
            AnnotationNode::Vertex *data = list.data();
            data += glyphCounter * 6;

            data[0] = { static_cast<float>(pos.x()),
                        static_cast<float>(pos.y()),
                        static_cast<float>(glyphPos.x()),
                        static_cast<float>(glyphPos.y()),
                        static_cast<float>(texture.getLeft()),
                        static_cast<float>(texture.getTop()),
                        static_cast<uchar>(color.red()),
                        static_cast<uchar>(color.green()),
                        static_cast<uchar>(color.blue()),
                        static_cast<uchar>(color.alpha()),
                        minZoom };

            data[1] = { static_cast<float>(pos.x()),
                        static_cast<float>(pos.y()),
                        static_cast<float>(glyphPos.x() + glyphWidth),
                        static_cast<float>(glyphPos.y()),
                        static_cast<float>(texture.getRight()),
                        static_cast<float>(texture.getTop()),
                        static_cast<uchar>(color.red()),
                        static_cast<uchar>(color.green()),
                        static_cast<uchar>(color.blue()),
                        static_cast<uchar>(color.alpha()),
                        minZoom };

            data[2] = { static_cast<float>(pos.x()),
                        static_cast<float>(pos.y()),
                        static_cast<float>(glyphPos.x()),
                        static_cast<float>(glyphPos.y() + glyphHeight),
                        static_cast<float>(texture.getLeft()),
                        static_cast<float>(texture.getBottom()),
                        static_cast<uchar>(color.red()),
                        static_cast<uchar>(color.green()),
                        static_cast<uchar>(color.blue()),
                        static_cast<uchar>(color.alpha()),
                        minZoom };

            data[3] = { static_cast<float>(pos.x()),
                        static_cast<float>(pos.y()),
                        static_cast<float>(glyphPos.x()),
                        static_cast<float>(glyphPos.y() + glyphHeight),
                        static_cast<float>(texture.getLeft()),
                        static_cast<float>(texture.getBottom()),
                        static_cast<uchar>(color.red()),
                        static_cast<uchar>(color.green()),
                        static_cast<uchar>(color.blue()),
                        static_cast<uchar>(color.alpha()),
                        minZoom };

            data[4] = { static_cast<float>(pos.x()),
                        static_cast<float>(pos.y()),
                        static_cast<float>(glyphPos.x() + glyphWidth),
                        static_cast<float>(glyphPos.y()),
                        static_cast<float>(texture.getRight()),
                        static_cast<float>(texture.getTop()),
                        static_cast<uchar>(color.red()),
                        static_cast<uchar>(color.green()),
                        static_cast<uchar>(color.blue()),
                        static_cast<uchar>(color.alpha()),
                        minZoom };

            data[5] = { static_cast<float>(pos.x()),
                        static_cast<float>(pos.y()),
                        static_cast<float>(glyphPos.x() + glyphWidth),
                        static_cast<float>(glyphPos.y() + glyphHeight),
                        static_cast<float>(texture.getRight()),
                        static_cast<float>(texture.getBottom()),
                        static_cast<uchar>(color.red()),
                        static_cast<uchar>(color.green()),
                        static_cast<uchar>(color.blue()),
                        static_cast<uchar>(color.alpha()),
                        minZoom };

            glyphCounter++;
        }
    }

    Q_ASSERT(glyphCounter * 6 == list.size());
    return list;
}

QRect getMercatorRegion(const GeoRect &tileRect)
{
    int left = Mercator::mercatorWidth(0, tileRect.left(), s_pixelsPerLon);
    int right = Mercator::mercatorWidth(0, tileRect.right(), s_pixelsPerLon);
    int top = Mercator::mercatorHeight(0, tileRect.top(), s_pixelsPerLon);
    int bottom = Mercator::mercatorHeight(0, tileRect.bottom(), s_pixelsPerLon);
    return QRect(left, top, right - left, bottom - top);
}

/*!
    Fetch data from the tilefactory and converts to vertex data
*/
TileData fetchData(TileFactoryWrapper *tileFactory,
                   TileFactoryWrapper::TileRecipe recipe,
                   std::shared_ptr<const SymbolImage> symbolImage,
                   std::shared_ptr<const FontImage> fontImage)
{
    Q_ASSERT(tileFactory);

    std::vector<std::shared_ptr<Chart>> charts;

    try {
        charts = tileFactory->create(recipe);
    } catch (const std::exception &e) {
        qWarning() << "Exception in tilefactory: " << e.what();
        return {};
    }

    Annotater annotater(fontImage, symbolImage, s_pixelsPerLon);
    Annotater::Annotations annotations = annotater.getAnnotations(charts);
    std::vector<AnnotationSymbol> &symbols = annotations.symbols;

    float maxZoom = recipe.pixelsPerLongitude / s_pixelsPerLon;

    ZoomSweeper zoomSweeper(maxZoom, getMercatorRegion(recipe.rect));
    zoomSweeper.calcSymbols(annotations.symbols);
    zoomSweeper.calcLabels(annotations.symbols, annotations.labels);

    TileData tileData;
    tileData.symbolVertices = getSymbolVertices(annotations.symbols);
    tileData.textVertices = getTextVertices(annotations.labels, fontImage.get());

    int clippingMarginInPixels = 4;

    double longitudeMargin = Mercator::mercatorWidthInverse(recipe.rect.left(),
                                                            clippingMarginInPixels,
                                                            recipe.pixelsPerLongitude)
        - recipe.rect.left();
    double latitudeMargin = recipe.rect.top()
        - Mercator::mercatorHeightInverse(recipe.rect.top(),
                                          clippingMarginInPixels,
                                          recipe.pixelsPerLongitude);

    GeoRect lineClippingRect(recipe.rect.top() + latitudeMargin,
                             recipe.rect.bottom() - latitudeMargin,
                             recipe.rect.left() - longitudeMargin,
                             recipe.rect.right() + longitudeMargin);

    for (const std::shared_ptr<Chart> &chart : charts) {

        GeometryLayer geometryLayer;
        float zBase = 1;

        geometryLayer.polygonVertices += drawPolygons<ChartData::CoverageArea>(
            chart->coverage(),
            zBase - 0.1,
            [](const ChartData::CoverageArea::Reader &area) -> QColor {
                return Qt::white;
            });

        geometryLayer.polygonVertices += drawPolygons<ChartData::DepthArea>(
            chart->depthAreas(),
            zBase - 0.2,
            [](const ChartData::DepthArea::Reader &depthArea) -> QColor {
                float depth = depthArea.getDepth();
                if (depth < 0.5) {
                    return criticalWaterColor;
                }
                // The function below maps depth: [0.5, 32] m to a factor [54, 0].
                // Factor is subtracted from white meaning higher depth gives more white.
                // Because of the log function the lower range depths are emphasized.
                float factor = std::clamp<int>(30 - 30 * log10(depth) + 20, 0, 50);
                return QColor(255 - 2.0 * factor,
                              255 - 0.6 * factor,
                              255 - 0.2 * factor);
            });

        geometryLayer.polygonVertices += drawPolygons<ChartData::LandArea>(
            chart->landAreas(),
            zBase - 0.5,
            [](const ChartData::LandArea::Reader &landArea) -> QColor {
                return landAreaColor;
            });

        geometryLayer.polygonVertices += drawPolygons<ChartData::BuiltUpArea>(
            chart->builtUpAreas(),
            zBase - 0.7,
            [](const ChartData::BuiltUpArea::Reader &depthArea) -> QColor {
                return builtUpAreaColor;
            });

        geometryLayer.polygonVertices += drawPolygons<ChartData::Road>(
            chart->roads(),
            zBase - 0.8,
            [](const ChartData::Road::Reader &road) -> QColor {
                return roadColor;
            });

        geometryLayer.polygonVertices += drawPolygons<ChartData::Pontoon>(
            chart->pontoons(),
            zBase - 0.8,
            [](const ChartData::Pontoon::Reader &pontoon) -> QColor {
                return pontoonColor;
            });

        GeometryLayer::LineGroup thinLines;
        thinLines.style.width = GeometryLayer::LineGroup::Style::Width::Thin;

        GeometryLayer::LineGroup mediumLines;
        mediumLines.style.width = GeometryLayer::LineGroup::Style::Width::Medium;

        GeometryLayer::LineGroup thickLines;
        thickLines.style.width = GeometryLayer::LineGroup::Style::Width::Thick;

        thinLines.vertices += strokePolygons<ChartData::Road>(
            chart->roads(),
            [](const ChartData::Road::Reader &road) -> QColor {
                return roadColorBorder;
            },
            lineClippingRect);

        thinLines.vertices += drawLines<ChartData::DepthContour>(
            chart->depthContours(),
            [](const ChartData::DepthContour::Reader &depthContour) -> QColor {
                return depthContourColor;
            });

        thickLines.vertices += drawLines<ChartData::Road>(
            chart->roads(),
            [](const ChartData::Road::Reader &road) -> QColor {
                return roadColor;
            });

        thickLines.vertices += drawLines<ChartData::Pontoon>(
            chart->pontoons(),
            [](const ChartData::Pontoon::Reader &pontoon) -> QColor {
                return pontoonColor;
            });

        mediumLines.vertices += drawLines<ChartData::CoastLine>(
            chart->coastLines(),
            [](const ChartData::CoastLine::Reader &coastLine) -> QColor {
                return coastLineColor;
            });

        geometryLayer.lineGroups.append(thinLines);
        geometryLayer.lineGroups.append(mediumLines);
        geometryLayer.lineGroups.append(thickLines);

        tileData.geometryLayers.append(geometryLayer);
    }

    return tileData;
}
}

Tessellator::Tessellator(TileFactoryWrapper *tileFactory,
                         TileFactoryWrapper::TileRecipe recipe,
                         std::shared_ptr<const SymbolImage> symbolImage,
                         std::shared_ptr<const FontImage> fontImage)
    : m_fontImage(fontImage)
    , m_symbolImage(symbolImage)
    , m_tileFactory(tileFactory)
    , m_recipe(recipe)
{
    connect(&m_watcher, &QFutureWatcher<TileData>::finished, this, &Tessellator::finished);
    m_data.geometryLayers.append(createLoadingIndicatorLayer());
}

void Tessellator::setId(const QString &id)
{
    m_id = id;
}

int Tessellator::pixelsPerLon()
{
    return s_pixelsPerLon;
}

void Tessellator::setTileFactory(TileFactoryWrapper *tileFactory)
{
    if (m_tileFactory == tileFactory) {
        return;
    }
    m_tileFactory = tileFactory;
    fetchAgain();
}

void Tessellator::fetchAgain()
{
    if (!m_tileFactory) {
        return;
    }

    if (m_watcher.isRunning()) {
        m_fetchAgain = true;
        return;
    }

    m_result = QtConcurrent::run(fetchData,
                                 m_tileFactory,
                                 m_recipe,
                                 m_symbolImage,
                                 m_fontImage);
    m_watcher.setFuture(m_result);

    if (m_result.isFinished()) {
        finished();
    }
    m_fetchAgain = false;
}

void Tessellator::finished()
{
    if (m_fetchAgain) {
        fetchAgain();
    } else {
        m_ready = true;
        m_dataChanged = true;
        m_data = m_result.result();
        emit dataChanged(m_id);
    }
}

void Tessellator::setRemoved()
{
    m_removed = true;
}

QList<PolygonNode::Vertex> Tessellator::createTileVertices(const QColor &color) const
{
    QList<QPointF> points;
    points << posToMercator(m_recipe.rect.topLeft());
    points << posToMercator(m_recipe.rect.bottomLeft());
    points << posToMercator(m_recipe.rect.topRight());
    points << posToMercator(m_recipe.rect.topRight());
    points << posToMercator(m_recipe.rect.bottomLeft());
    points << posToMercator(m_recipe.rect.bottomRight());

    QList<PolygonNode::Vertex> vertices;
    vertices.resize(points.size());

    int vertexCount = 0;
    for (const auto &point : points) {
        vertices[vertexCount] = { static_cast<float>(point.x()),
                                  static_cast<float>(point.y()),
                                  0.5,
                                  static_cast<uchar>(color.red()),
                                  static_cast<uchar>(color.green()),
                                  static_cast<uchar>(color.blue()),
                                  static_cast<uchar>(color.alpha()) };
        vertexCount++;
    }
    return vertices;
}

GeometryLayer Tessellator::createLoadingIndicatorLayer() const
{
    GeometryLayer geometryLayer;
    geometryLayer.polygonVertices = createTileVertices(tilePlaceholderFillColor);

    const GeoRect &boundingBox = m_recipe.rect;

    QList<QPointF> linePoints = {
        posToMercator(boundingBox.topLeft()),
        posToMercator(boundingBox.bottomLeft()),
        posToMercator(boundingBox.bottomRight()),
        posToMercator(boundingBox.topRight()),
        posToMercator(boundingBox.topLeft())
    };

    GeometryLayer::LineGroup lineGroup;
    lineGroup.style.width = GeometryLayer::LineGroup::Style::Width::Thin;
    lineGroup.vertices = tessellateLine(linePoints, tilePlaceholderOutlineColor);
    geometryLayer.lineGroups.append(lineGroup);

    return geometryLayer;
}
