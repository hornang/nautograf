#include <QLocale>

#include <limits>

#include <freetype-gl/freetype-gl.h>
#include <freetype-gl/texture-font.h>

#include "tessellator.h"
#include "tilefactory/mercator.h"
#include "tilefactory/triangulator.h"

static const QColor labelColor = Qt::black;
static const QColor symbolLabelColor = Qt::black;
static const QColor criticalWaterColor(165, 202, 159);
static const QColor landAreaColor(255, 240, 190);
static const QColor builtUpAreaColor(240, 220, 160);
static const QColor coastLineColor = landAreaColor.darker(200);
static const QColor builtUpAreaColorBorder = builtUpAreaColor.darker(150);
static const QColor pontoonColor = builtUpAreaColor.darker(120);
static const QColor pontoonBorderColor = pontoonColor.darker(150);
static const QColor roadColor = landAreaColor.darker(120);
static const QColor roadColorBorder = roadColor.darker(150);

// Arbitrary chosen conversion factor to transform lat/lon to an internal mercator
// projection that can be linearly transformed in scene graph/vertex shader.
const int s_pixelsPerLon = 1000;

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

QRectF Tessellator::computeSymbolBox(const QTransform &transform,
                                     const QPointF &pos,
                                     SymbolImage::TextureSymbol &textureSymbol)
{
    const auto topLeft = transform.map(pos) - textureSymbol.center;
    return QRectF(topLeft + textureSymbol.roi.topLeft(),
                  textureSymbol.roi.size());
}

QPointF Tessellator::posToMercator(const ChartData::Position::Reader &pos)
{
    return { Mercator::mercatorWidth(0, pos.getLongitude(), s_pixelsPerLon),
             Mercator::mercatorHeight(0, pos.getLatitude(), s_pixelsPerLon) };
}

QPointF Tessellator::labelOffset(const QRectF &labelBox,
                                 const SymbolImage::TextureSymbol &symbol,
                                 LabelPlacement placement)
{
    const int labelMargin = 2;

    switch (placement) {
    case LabelPlacement::Below:
    default:
        return { -labelBox.width() / 2 - symbol.size.width() / 2 + symbol.center.x(),
                 symbol.size.height() - symbol.center.y() + labelMargin };
    }
}

TileData Tessellator::fetchData(TileFactoryWrapper *tileFactory,
                                TileFactoryWrapper::TileRecipe recipe,
                                std::shared_ptr<const SymbolImage> symbolImage,
                                std::shared_ptr<const FontImage> fontImage)
{
    Q_ASSERT(tileFactory);

    std::vector<std::shared_ptr<Chart>> charts;

    try {
        charts = tileFactory->create(recipe);
    } catch(const std::exception &e) {
        qWarning() << "Exception in tilefactory: " << e.what();
        return {};
    }

    TileData tileData;
    QList<Symbol> symbols;
    const auto locale = QLocale::system();

    const float soundingPointSize = 16;
    const float rockPointSize = 17;
    const float beaconPointSize = 20;


    const float landRegionPointSize = 16;
    const float builtUpPointSize = 22;
    const float landAreaPointSize = 22;
    const float builtUpAreaPointSize = 22;

    QElapsedTimer elapsedTimer;
    elapsedTimer.start();

    QList<SymbolLabel> labels;

    for (const auto &chart : charts) {
        for (const auto &sounding : chart->soundings()) {
            const auto pos = posToMercator(sounding.getPosition());

            int precision = 0;
            if (sounding.getDepth() < 30) {
                precision = 1;
            }

            const auto depthLabel = locale.toString(sounding.getDepth(), 'f', precision);
            const auto labelBox = fontImage->boundingBox(depthLabel,
                                                         soundingPointSize,
                                                         FontImage::FontType::Soundings);
            symbols.append({ pos,
                             std::nullopt,
                             0,
                             { { pos,
                                 depthLabel,
                                 QPointF(-labelBox.width() / 2, -labelBox.height() / 2),
                                 labelBox,
                                 FontImage::FontType::Soundings,
                                 QColor(120, 120, 120),
                                 soundingPointSize,
                                 std::nullopt,
                                 std::nullopt } },
                             0,
                             CollisionRule::NoCheck });
        }

        for (const auto &rock : chart->underwaterRocks()) {
            auto symbol = symbolImage->underwaterRock(rock);

            if (!symbol.has_value()) {
                continue;
            }

            int precision = 0;
            if (rock.getDepth() < 30) {
                precision = 1;
            }

            const auto pos = posToMercator(rock.getPosition());
            const auto depthLabel = locale.toString(rock.getDepth(), 'f', precision);
            const auto boundingBox = fontImage->boundingBox(depthLabel,
                                                         rockPointSize,
                                                         FontImage::FontType::Soundings);
            symbols.append({ pos,
                             symbol.value(),
                             std::nullopt,
                             { { pos,
                                 depthLabel,
                                 labelOffset(boundingBox, symbol.value(), LabelPlacement::Below),
                                 boundingBox,
                                 FontImage::FontType::Soundings,
                                 symbolLabelColor,
                                 rockPointSize,
                                 std::nullopt,
                                 std::nullopt } },
                             1,
                             CollisionRule::Always });
        }

        for (const auto &buoy : chart->lateralBuoys()) {
            auto symbol = symbolImage->lateralBuoy(buoy);
            if (!symbol.has_value()) {
                continue;
            }

            const auto pos = posToMercator(buoy.getPosition());

            // At some point this should be the color letter of the buoy
            const auto label("?");
            const auto labelBox = fontImage->boundingBox(label,
                                                         soundingPointSize,
                                                         FontImage::FontType::Normal);
            symbols.append({ pos,
                             symbol.value(),
                             std::nullopt,
                             { { pos,
                                 label,
                                 labelOffset(labelBox, symbol.value(), LabelPlacement::Below),
                                 labelBox,
                                 FontImage::FontType::Normal,
                                 symbolLabelColor,
                                 soundingPointSize,
                                 std::nullopt } },
                             2,
                             CollisionRule::OnlyWithSameType });
        }

        for (const auto &item : chart->landRegions()) {
            const auto pos = posToMercator(item.getPosition());
            const auto name = QString::fromStdString(item.getName());
            const auto labelBox = fontImage->boundingBox(name,
                                                         landRegionPointSize,
                                                         FontImage::FontType::Normal);
            symbols.append({ pos,
                             std::nullopt,
                             std::nullopt,
                             { { pos,
                                 name,
                                 QPointF(-labelBox.width() / 2, -labelBox.height() / 2),
                                 labelBox,
                                 FontImage::FontType::Normal,
                                 labelColor,
                                 landRegionPointSize,
                                 std::nullopt } },
                             3,
                             CollisionRule::Always });
        }

        for (const auto &item : chart->builtUpPoints()) {
            const auto &position = item.getPosition();
            const auto x = Mercator::mercatorWidth(0, position.getLongitude(), s_pixelsPerLon);
            const auto y = Mercator::mercatorHeight(0, position.getLatitude(), s_pixelsPerLon);
            const auto name = QString::fromStdString(item.getName());

            const auto labelBox = fontImage->boundingBox(name,
                                                         builtUpPointSize,
                                                         FontImage::FontType::Normal);
            const QPointF pos(x, y);
            symbols.append({ pos,
                             std::nullopt,
                             std::nullopt,
                             { { pos,
                                 name,
                                 QPointF(-labelBox.width() / 2, -labelBox.height() / 2),
                                 labelBox,
                                 FontImage::FontType::Normal,
                                 labelColor,
                                 builtUpPointSize,
                                 std::nullopt } },
                             4,
                             CollisionRule::Always });
        }

        for (const auto &item : chart->landAreas()) {
            if (item.getName().size() == 0) {
                continue;
            }

            const auto &position = item.getCentroid();

            // There is no inter-tile collision detection. Avoid showing labels
            // with a position outside this tile. Shouldn't labels clipped out in chart.cpp?
            if (!recipe.rect.contains(position.getLatitude(), position.getLongitude())) {
                continue;
            }

            const auto x = Mercator::mercatorWidth(0, position.getLongitude(), s_pixelsPerLon);
            const auto y = Mercator::mercatorHeight(0, position.getLatitude(), s_pixelsPerLon);
            const auto name = QString::fromStdString(item.getName());

            const auto labelBox = fontImage->boundingBox(name,
                                                         landAreaPointSize,
                                                         FontImage::FontType::Normal);
            const QPointF pos(x, y);
            symbols.append({ pos,
                             std::nullopt,
                             std::nullopt,
                             { { pos,
                                 name,
                                 QPointF(-labelBox.width() / 2, -labelBox.height() / 2),
                                 labelBox,
                                 FontImage::FontType::Normal,
                                 symbolLabelColor,
                                 landAreaPointSize,
                                 std::nullopt } },
                             4,
                             CollisionRule::Always });
        }

        for (const auto &item : chart->builtUpAreas()) {
            if (item.getName().size() == 0) {
                continue;
            }

            const auto &position = item.getCentroid();

            // There is no inter-tile collision detection. Avoid showing labels
            // with a position outside this tile. Shouldn't labels clipped out in chart.cpp?
            if (!recipe.rect.contains(position.getLatitude(), position.getLongitude())) {
                continue;
            }

            const auto x = Mercator::mercatorWidth(0, position.getLongitude(), s_pixelsPerLon);
            const auto y = Mercator::mercatorHeight(0, position.getLatitude(), s_pixelsPerLon);
            const auto name = QString::fromStdString(item.getName());

            const auto metrics = fontImage->boundingBox(name,
                                                        builtUpAreaPointSize,
                                                        FontImage::FontType::Normal);
            const QPointF labelOffset(-metrics.width() / 2, 0);
            const QPointF pos(x, y);
            symbols.append({ pos,
                             std::nullopt,
                             std::nullopt,
                             { { pos,
                                 name,
                                 labelOffset,
                                 metrics,
                                 FontImage::FontType::Normal,
                                 symbolLabelColor,
                                 builtUpAreaPointSize,
                                 std::nullopt } },
                             4,
                             CollisionRule::Always });
        }

        for (const auto &beacon : chart->beacons()) {
            auto symbol = symbolImage->beacon(beacon);

            if (!symbol.has_value()) {
                continue;
            }

            const auto pos = posToMercator(beacon.getPosition());
            const QPoint symbolSize(symbol->size.width(), symbol->size.height());
            const auto name = QString::fromStdString(beacon.getName());
            const auto metrics = fontImage->boundingBox(name,
                                                        beaconPointSize,
                                                        FontImage::FontType::Normal);
            symbols.append({ pos,
                             symbol.value(),
                             std::nullopt,
                             { { pos,
                                 name,
                                 labelOffset(metrics, symbol.value(), LabelPlacement::Below),
                                 metrics,
                                 FontImage::FontType::Normal,
                                 symbolLabelColor,
                                 beaconPointSize,
                                 std::nullopt } },
                             5,
                             CollisionRule::Always });
        }
    }

    std::sort(symbols.begin(), symbols.end(), [](const Symbol &a, const Symbol &b) {
        return a.priority > b.priority;
    });

    float maxScale = recipe.pixelsPerLongitude / s_pixelsPerLon;

    const auto leftX = Mercator::mercatorWidth(0, recipe.rect.left(), s_pixelsPerLon);
    const auto topY = Mercator::mercatorHeight(0, recipe.rect.top(), s_pixelsPerLon);

    const float zoomRatios[] = { 5, 4, 3, 2, 1, 0 };
    QTransform transforms[std::size(zoomRatios)];

    int i = 0;
    for (const auto &zoomExp : zoomRatios) {
        const float scale = maxScale / pow(2, zoomExp / 5);

        QTransform transform;
        transform.scale(scale, scale);
        transform.translate(-leftX, -topY);
        transforms[i++] = transform;
    }

    struct SymbolBox
    {
        QRectF box;
        SymbolImage::TextureSymbol symbol;
    };

    // Place symbols
    for (const auto &transform : transforms) {
        const auto scale = transform.m11();

        QList<SymbolBox> existingBoxes;

        // Create collision rectangles for symbols already shown at smaller scale
        for (auto &symbol : symbols) {
            if (symbol.symbol.has_value() && symbol.scaleLimit.has_value()) {
                auto a = computeSymbolBox(transform,
                                          symbol.pos,
                                          symbol.symbol.value());
                existingBoxes.append(SymbolBox { a, symbol.symbol.value() });
            }
        }

        for (auto &symbol : symbols) {
            if (!symbol.symbol.has_value()) {
                // This symbol has no actual symbol so we set allowed scale
                // to any scale so that child labels can be shown. This sitation
                // is currently used to shown text labels which doesnt have a
                // parent symbol.
                symbol.scaleLimit = 0;
                continue;
            }

            if (symbol.scaleLimit.has_value()) {
                // This symbol was already set to be shown at smaller scale
                continue;
            }

            SymbolBox symbolBox { computeSymbolBox(transform,
                                                   symbol.pos,
                                                   symbol.symbol.value()),
                                  symbol.symbol.value() };

            bool collision = false;

            if (symbol.collisionRule != CollisionRule::NoCheck) {
                for (const auto &other : existingBoxes) {
                    if (symbolBox.box.intersects(other.box)
                        && (symbol.collisionRule == CollisionRule::Always || other.symbol == symbolBox.symbol)) {
                        collision = true;
                        break;
                    }
                }
            }

            if (!collision) {
                symbol.scaleLimit = scale;
                existingBoxes.append(symbolBox);
            }
        }
    }

    for (auto &symbol : symbols) {
        for (auto &label : symbol.labels) {
            label.parentScaleLimit = symbol.scaleLimit;
            labels.append(label);
        }
    }

    // Place labels
    for (const auto &transform : transforms) {
        const auto scale = transform.m11();

        QList<QRectF> boxes;

        for (auto &symbol : symbols) {
            if (symbol.symbol.has_value() && symbol.scaleLimit.has_value()) {
                const auto pos = transform.map(symbol.pos) - symbol.symbol.value().center;
                boxes.append(QRectF(pos, symbol.symbol.value().size));
            }
        }

        for (auto &label : labels) {
            if (label.scaleLimit.has_value()) {
                const auto pos = transform.map(label.pos) + label.offset;
                boxes.append(QRectF(pos, label.boundingBox.size()));
            }
        }

        for (auto &label : labels) {
            if (!label.parentScaleLimit.has_value() || scale < label.parentScaleLimit.value()) {
                continue;
            }

            const auto pos = transform.map(label.pos) + label.offset;
            QRectF labelBox(pos, label.boundingBox.size());

            bool collision = false;

            for (const auto &collisionRect : boxes) {
                if (labelBox.intersects(collisionRect)) {
                    collision = true;
                    break;
                }
            }

            if (!collision) {
                label.scaleLimit = scale;
                boxes.append(labelBox);
            }
        }
    }

    tileData.symbolVertices = addSymbols(symbols);
    tileData.textVertices = addLabels(labels, fontImage.get());

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
            zBase - 0.8,
            [](const ChartData::BuiltUpArea::Reader &depthArea) -> QColor {
                return builtUpAreaColor;
            });
        tileData.geometryLayers.append(geometryLayer);
    }

    return tileData;
}

QList<AnnotationNode::Vertex> Tessellator::addLabels(const QList<SymbolLabel> &labels,
                                                     const FontImage *fontImage)
{
    QList<AnnotationNode::Vertex> list;
    int glyphCounter = 0;

    for (const auto &label : labels) {
        if (!label.scaleLimit.has_value()) {
            continue;
        }

        float scaleLimit = label.scaleLimit.value();

        auto glyphs = fontImage->glyphs(label.text,
                                        label.pointSize,
                                        label.font);
        const QColor &color = label.color;

        for (const auto glyph : glyphs) {
            const QPointF metricsOffset(0, -label.boundingBox.top());
            QPointF glyphPos = glyph.target.topLeft();
            glyphPos += label.offset + metricsOffset;
            QRectF texture = glyph.texture;
            const auto glyphWidth = static_cast<float>(glyph.target.width());
            const auto glyphHeight = static_cast<float>(glyph.target.height());
            const auto &pos = label.pos;

            list.resize(list.size() + 6);
            AnnotationNode::Vertex *data = list.data();
            data += glyphCounter * 6;

            data[0] = { static_cast<float>(pos.x()),
                        static_cast<float>(pos.y()),
                        static_cast<float>(glyphPos.x()),
                        static_cast<float>(glyphPos.y()),
                        static_cast<float>(texture.left()),
                        static_cast<float>(texture.top()),
                        static_cast<uchar>(color.red()),
                        static_cast<uchar>(color.green()),
                        static_cast<uchar>(color.blue()),
                        static_cast<uchar>(color.alpha()),
                        scaleLimit };

            data[1] = { static_cast<float>(pos.x()),
                        static_cast<float>(pos.y()),
                        static_cast<float>(glyphPos.x() + glyphWidth),
                        static_cast<float>(glyphPos.y()),
                        static_cast<float>(texture.right()),
                        static_cast<float>(texture.top()),
                        static_cast<uchar>(color.red()),
                        static_cast<uchar>(color.green()),
                        static_cast<uchar>(color.blue()),
                        static_cast<uchar>(color.alpha()),
                        scaleLimit };

            data[2] = { static_cast<float>(pos.x()),
                        static_cast<float>(pos.y()),
                        static_cast<float>(glyphPos.x()),
                        static_cast<float>(glyphPos.y() + glyphHeight),
                        static_cast<float>(texture.left()),
                        static_cast<float>(texture.bottom()),
                        static_cast<uchar>(color.red()),
                        static_cast<uchar>(color.green()),
                        static_cast<uchar>(color.blue()),
                        static_cast<uchar>(color.alpha()),
                        scaleLimit };

            data[3] = { static_cast<float>(pos.x()),
                        static_cast<float>(pos.y()),
                        static_cast<float>(glyphPos.x()),
                        static_cast<float>(glyphPos.y() + glyphHeight),
                        static_cast<float>(texture.left()),
                        static_cast<float>(texture.bottom()),
                        static_cast<uchar>(color.red()),
                        static_cast<uchar>(color.green()),
                        static_cast<uchar>(color.blue()),
                        static_cast<uchar>(color.alpha()),
                        scaleLimit };

            data[4] = { static_cast<float>(pos.x()),
                        static_cast<float>(pos.y()),
                        static_cast<float>(glyphPos.x() + glyphWidth),
                        static_cast<float>(glyphPos.y()),
                        static_cast<float>(texture.right()),
                        static_cast<float>(texture.top()),
                        static_cast<uchar>(color.red()),
                        static_cast<uchar>(color.green()),
                        static_cast<uchar>(color.blue()),
                        static_cast<uchar>(color.alpha()),
                        scaleLimit };

            data[5] = { static_cast<float>(pos.x()),
                        static_cast<float>(pos.y()),
                        static_cast<float>(glyphPos.x() + glyphWidth),
                        static_cast<float>(glyphPos.y() + glyphHeight),
                        static_cast<float>(texture.right()),
                        static_cast<float>(texture.bottom()),
                        static_cast<uchar>(color.red()),
                        static_cast<uchar>(color.green()),
                        static_cast<uchar>(color.blue()),
                        static_cast<uchar>(color.alpha()),
                        scaleLimit };

            glyphCounter++;
        }
    }

    Q_ASSERT(glyphCounter * 6 == list.size());
    return list;
}

QList<AnnotationNode::Vertex> Tessellator::addSymbols(const QList<Symbol> &input)
{
    int validSymbols = 0;

    for (const Symbol &element : input) {
        if (element.symbol.has_value()) {
            validSymbols++;
        }
    }

    QList<AnnotationNode::Vertex> vertices(validSymbols * 6);
    AnnotationNode::Vertex *data = vertices.data();

    for (const auto &element : input) {
        if (!element.symbol.has_value()) {
            continue;
        }

        const auto &symbol = element.symbol.value();
        const auto &center = symbol.center;
        const auto &pos = element.pos;
        const auto &sourceRect = symbol.coords;
        const auto &symbolSize = symbol.size;
        const auto &color = symbol.color;

        float scaleLimit = 1000;

        if (element.scaleLimit) {
            scaleLimit = element.scaleLimit.value();
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
                    scaleLimit };

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
                    scaleLimit };

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
                    scaleLimit };

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
                    scaleLimit };

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
                    scaleLimit };

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
                    scaleLimit };

        data += 6;
    }

    return vertices;
}

template <typename T>
QList<PolygonNode::Vertex> Tessellator::drawPolygons(const typename capnp::List<T>::Reader &areas,
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

QPointF Tessellator::posToMercator(const Pos &pos)
{
    return { Mercator::mercatorWidth(0, pos.lon(), s_pixelsPerLon),
             Mercator::mercatorHeight(0, pos.lat(), s_pixelsPerLon) };
}

QList<PolygonNode::Vertex> Tessellator::overlayVertices(const QColor &color) const
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
                                  50 };
        vertexCount++;
    }
    return vertices;
}
