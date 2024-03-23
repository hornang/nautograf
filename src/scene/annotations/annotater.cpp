#include <memory>

#include <QRectF>
#include <QString>

#include "annotater.h"
#include "scene/annotations/fontimage.h"
#include "symbolimage.h"
#include "tilefactory/mercator.h"

using namespace std;

namespace {

const QColor symbolLabelColor = Qt::black;
const QColor labelColor = Qt::black;

enum class LabelPlacement {
    Below,
};

QPointF getLabelOffset(const QRectF &labelBox,
                       const TextureSymbol &symbol,
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
}

Annotater::Annotater(shared_ptr<const FontImage> &fontImage,
                     shared_ptr<const SymbolImage> &symbolImage,
                     int pixelsPerLon)
    : m_fontImage(fontImage)
    , m_symbolImage(symbolImage)
    , m_pixelsPerLon(pixelsPerLon)
{
}

QPointF Annotater::posToMercator(const ChartData::Position::Reader &pos) const
{
    return { Mercator::mercatorWidth(0, pos.getLongitude(), m_pixelsPerLon),
             Mercator::mercatorHeight(0, pos.getLatitude(), m_pixelsPerLon) };
}

QString Annotater::getDepthString(float depth) const
{
    return m_locale.toString(depth);
}

template <typename T>
Annotater::Annotations Annotater::getAnnotations(
    const typename capnp::List<T>::Reader &elements,
    function<optional<TextureSymbol>(const typename T::Reader &)> getSymbol,
    function<optional<ChartData::Position::Reader>(const typename T::Reader &)> getPosition,
    function<Label(const typename T::Reader &)> getLabel,
    int priority,
    CollisionRule collissionRule) const

{
    assert(getPosition);

    vector<AnnotationSymbol> symbols;
    vector<AnnotationLabel> labels;

    for (const auto &element : elements) {
        const optional<ChartData::Position::Reader> maybePosition = getPosition(element);

        if (!maybePosition.has_value()) {
            continue;
        }

        const auto pos = posToMercator(maybePosition.value());

        Label label = getLabel(element);
        const auto labelBoundingBox = m_fontImage->boundingBox(label.text,
                                                               label.pointSize,
                                                               label.font);

        optional<TextureSymbol> symbol;

        if (getSymbol != nullptr) {
            symbol = getSymbol(element);

            if (!symbol.has_value()) {
                continue;
            }
        }

        if (!symbol.has_value() && label.text.isEmpty()) {
            continue;
        }

        QPointF labelOffset;

        optional<size_t> parentSymbolIndex;

        if (symbol.has_value()) {
            labelOffset = getLabelOffset(labelBoundingBox, symbol.value(), LabelPlacement::Below);
            symbols.push_back(AnnotationSymbol { pos,
                                                 symbol,
                                                 nullopt,
                                                 priority,
                                                 collissionRule });
            parentSymbolIndex = symbols.size() - 1;
        } else {
            labelOffset = QPointF(-labelBoundingBox.width() / 2, -labelBoundingBox.height() / 2);
        }

        labels.push_back(AnnotationLabel { label,
                                           pos,
                                           parentSymbolIndex,
                                           labelOffset,
                                           labelBoundingBox });
    }

    return { symbols, labels };
}

Annotater::Annotations Annotater::getAnnotations(const vector<shared_ptr<Chart>> &charts)
{
    const float soundingPointSize = 16;
    const float rockPointSize = 17;
    const float beaconPointSize = 20;

    const float landRegionPointSize = 16;
    const float builtUpPointSize = 22;
    const float landAreaPointSize = 22;
    const float builtUpAreaPointSize = 22;

    Annotater::Annotations annotations;

    for (const auto &chart : charts) {
        annotations += getAnnotations<ChartData::Sounding>(
            chart->soundings(),
            nullptr,
            [](const auto &item) { return item.getPosition(); },
            [&](const auto &item) {
                return Label { getDepthString(item.getDepth()),
                               FontType::Soundings,
                               QColor(120, 120, 120),
                               soundingPointSize };
            },
            0,
            CollisionRule::NoCheck);

        annotations += getAnnotations<ChartData::UnderwaterRock>(
            chart->underwaterRocks(),
            [&](const auto &item) {
                return m_symbolImage->underwaterRock(item);
            },
            [](const auto &item) { return item.getPosition(); },
            [&](const auto &rock) {
                return Label { getDepthString(rock.getDepth()),
                               FontType::Soundings,
                               symbolLabelColor,
                               rockPointSize };
            },
            1,
            CollisionRule::Always);

        annotations += getAnnotations<ChartData::BuoyLateral>(
            chart->lateralBuoys(),
            [&](const auto &item) {
                return m_symbolImage->lateralBuoy(item);
            },
            [](const auto &item) { return item.getPosition(); },
            [&](const auto &item) {
                // At some point this should be the color letter of the buoy
                const auto label = QStringLiteral("?");

                return Label { label,
                               FontType::Normal,
                               symbolLabelColor,
                               soundingPointSize };
            },
            2,
            CollisionRule::OnlyWithSameType);

        annotations += getAnnotations<ChartData::LandRegion>(
            chart->landRegions(),
            nullptr,
            [](const auto &item) { return item.getPosition(); },
            [&](const auto &item) {
                return Label { QString::fromStdString(item.getName()),
                               FontType::Normal,
                               labelColor,
                               landRegionPointSize };
            },
            3,
            CollisionRule::Always);

        annotations += getAnnotations<ChartData::BuiltUpPoint>(
            chart->builtUpPoints(),
            nullptr,
            [](const auto &item) { return item.getPosition(); },
            [&](const auto &item) {
                return Label { QString::fromStdString(item.getName()),
                               FontType::Normal,
                               labelColor,
                               builtUpPointSize };
            },
            4,
            CollisionRule::Always);

        annotations += getAnnotations<ChartData::LandArea>(
            chart->landAreas(),
            nullptr,
            [](const auto &item) { return item.getCentroid(); },
            [&](const auto &item) {
                return Label { QString::fromStdString(item.getName()),
                               FontType::Normal,
                               symbolLabelColor,
                               landAreaPointSize };
            },
            4,
            CollisionRule::Always);

        annotations += getAnnotations<ChartData::BuiltUpArea>(
            chart->builtUpAreas(),
            nullptr,
            [](const auto &item) { return item.getCentroid(); },
            [&](const auto &item) {
                return Label { QString::fromStdString(item.getName()),
                               FontType::Normal,
                               symbolLabelColor,
                               builtUpAreaPointSize };
            },
            4,
            CollisionRule::Always);

        annotations += getAnnotations<ChartData::Beacon>(
            chart->beacons(),
            [&](const auto &item) { return m_symbolImage->beacon(item); },
            [](const auto &item) { return item.getPosition(); },
            [&](const auto &item) {
                return Label { QString::fromStdString(item.getName()),
                               FontType::Normal,
                               symbolLabelColor,
                               beaconPointSize };
            },
            5,
            CollisionRule::Always);
    }

    return annotations;
}
