﻿#include <QLocale>

#include <limits>

#include <freetype-gl/freetype-gl.h>
#include <freetype-gl/texture-font.h>

#include "tessellator.h"
#include "tilefactory/mercator.h"

// Arbitrary chosen conversion factor to transform lat/lon to an internal mercator
// projection that can be linearly transformed in scene graph/vertex shader.
const int s_pixelsPerLon = 1000;

// This number is tightly connected with annotationnode.cpp. annotationnode.cpp
// defines the number of floats per vertex which is 10. For displaying rectangles
// of chart symbols or individual glyphs there are two triangles and therefore
// 6 corners.
const int s_stride = 60;

Tessellator::Tessellator(TileFactoryWrapper *tileFactory,
                         TileFactoryWrapper::TileRecipe recipe,
                         const SymbolImage *symbolImage,
                         const FontImage *fontImage)
    : m_fontImage(fontImage)
    , m_symbolImage(symbolImage)
    , m_tileFactory(tileFactory)
    , m_recipe(recipe)
{
    connect(&m_watcher, &QFutureWatcher<TileData>::finished, this, &Tessellator::finished);
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
        emit dataChanged();
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

QPointF Tessellator::posToMeractor(const ChartData::Position::Reader &pos)
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

Tessellator::TileData Tessellator::fetchData(TileFactoryWrapper *tileFactory,
                                             TileFactoryWrapper::TileRecipe recipe,
                                             const SymbolImage *symbolImage,
                                             const FontImage *fontImage)
{
    Q_ASSERT(tileFactory);
    auto charts = tileFactory->create(recipe);

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
            const auto pos = posToMeractor(sounding.getPosition());

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

            const auto pos = posToMeractor(rock.getPosition());
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
                                 QColor(0, 0, 0),
                                 rockPointSize,
                                 std::nullopt,
                                 std::nullopt } },
                             1,
                             // This makes underwater rocks beeing show even if they collide with other symbols
                             CollisionRule::OnlyWithSameType });
        }

        for (const auto &buoy : chart->lateralBuoys()) {
            auto symbol = symbolImage->lateralBuoy(buoy);
            if (!symbol.has_value()) {
                continue;
            }

            const auto pos = posToMeractor(buoy.getPosition());

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
                                 Qt::black,
                                 soundingPointSize,
                                 std::nullopt } },
                             2,
                             CollisionRule::OnlyWithSameType });
        }

        for (const auto &item : chart->landRegions()) {
            const auto pos = posToMeractor(item.getPosition());
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
                                 Qt::black,
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
                                 Qt::black,
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
                                 Qt::black,
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
                                 Qt::black,
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

            const auto pos = posToMeractor(beacon.getPosition());
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
                                 Qt::black,
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
    tileData.textVertices = addLabels(labels, fontImage);


    return tileData;
}

QList<float> Tessellator::addLabels(const QList<SymbolLabel> &labels,
                                    const FontImage *fontImage)
{
    QList<float> list;
    int glyphCounter = 0;

    for (const auto &label : labels) {
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

            list.resize(list.size() + s_stride);
            float *data = list.data();
            data += glyphCounter * s_stride;

            float scaleLimit = 1000;

            if (label.scaleLimit.has_value()) {
                scaleLimit = label.scaleLimit.value();
            }

            data[0] = pos.x();
            data[1] = pos.y();
            data[2] = glyphPos.x();
            data[3] = glyphPos.y();
            data[4] = texture.left();
            data[5] = texture.top();
            data[6] = color.redF();
            data[7] = color.greenF();
            data[8] = color.blueF();
            data[9] = scaleLimit;

            data[10] = pos.x();
            data[11] = pos.y();
            data[12] = glyphPos.x() + glyphWidth;
            data[13] = glyphPos.y();
            data[14] = texture.right();
            data[15] = texture.top();
            data[16] = color.redF();
            data[17] = color.greenF();
            data[18] = color.blueF();
            data[19] = scaleLimit;

            data[20] = pos.x();
            data[21] = pos.y();
            data[22] = glyphPos.x();
            data[23] = glyphPos.y() + glyphHeight;
            data[24] = texture.left();
            data[25] = texture.bottom();
            data[26] = color.redF();
            data[27] = color.greenF();
            data[28] = color.blueF();
            data[29] = scaleLimit;

            data[30] = pos.x();
            data[31] = pos.y();
            data[32] = glyphPos.x();
            data[33] = glyphPos.y() + glyphHeight;
            data[34] = texture.left();
            data[35] = texture.bottom();
            data[36] = color.redF();
            data[37] = color.greenF();
            data[38] = color.blueF();
            data[39] = scaleLimit;

            data[40] = pos.x();
            data[41] = pos.y();
            data[42] = glyphPos.x() + glyphWidth;
            data[43] = glyphPos.y();
            data[44] = texture.right();
            data[45] = texture.top();
            data[46] = color.redF();
            data[47] = color.greenF();
            data[48] = color.blueF();
            data[49] = scaleLimit;

            data[50] = pos.x();
            data[51] = pos.y();
            data[52] = glyphPos.x() + glyphWidth;
            data[53] = glyphPos.y() + glyphHeight;
            data[54] = texture.right();
            data[55] = texture.bottom();
            data[56] = color.redF();
            data[57] = color.greenF();
            data[58] = color.blueF();
            data[59] = scaleLimit;

            glyphCounter++;
        }
    }

    Q_ASSERT(glyphCounter * s_stride == list.size());
    return list;
}

QList<float> Tessellator::addSymbols(const QList<Symbol> &input)
{
    QList<float> vertices(input.size() * s_stride);
    float *data = vertices.data();

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

        data[0] = pos.x();
        data[1] = pos.y();
        data[2] = -center.x();
        data[3] = -center.y();
        data[4] = sourceRect.left();
        data[5] = sourceRect.top();
        data[6] = color.redF();
        data[7] = color.greenF();
        data[8] = color.blueF();
        data[9] = scaleLimit;

        data[10] = pos.x();
        data[11] = pos.y();
        data[12] = -center.x() + symbolSize.width();
        data[13] = -center.y();
        data[14] = sourceRect.right();
        data[15] = sourceRect.top();
        data[16] = color.redF();
        data[17] = color.greenF();
        data[18] = color.blueF();
        data[19] = scaleLimit;

        data[20] = pos.x();
        data[21] = pos.y();
        data[22] = -center.x();
        data[23] = -center.y() + symbolSize.height();
        data[24] = sourceRect.left();
        data[25] = sourceRect.bottom();
        data[26] = color.redF();
        data[27] = color.greenF();
        data[28] = color.blueF();
        data[29] = scaleLimit;

        data[30] = pos.x();
        data[31] = pos.y();
        data[32] = -center.x();
        data[33] = -center.y() + symbolSize.height();
        data[34] = sourceRect.left();
        data[35] = sourceRect.bottom();
        data[36] = color.redF();
        data[37] = color.greenF();
        data[38] = color.blueF();
        data[39] = scaleLimit;

        data[40] = pos.x();
        data[41] = pos.y();
        data[42] = -center.x() + symbolSize.width();
        data[43] = -center.y();
        data[44] = sourceRect.right();
        data[45] = sourceRect.top();
        data[46] = color.redF();
        data[47] = color.greenF();
        data[48] = color.blueF();
        data[49] = scaleLimit;

        data[50] = pos.x();
        data[51] = pos.y();
        data[52] = -center.x() + symbolSize.width();
        data[53] = -center.y() + symbolSize.height();
        data[54] = sourceRect.right();
        data[55] = sourceRect.bottom();
        data[56] = color.redF();
        data[57] = color.greenF();
        data[58] = color.blueF();
        data[59] = scaleLimit;

        data += s_stride;
    }

    return vertices;
}