#include <QList>
#include <QPointF>
#include <QRectF>
#include <QTransform>

#include <quadtree/Box.h>
#include <quadtree/Quadtree.h>

#include "annotater.h"
#include "zoomsweeper.h"

using namespace std;

namespace {
quadtree::Box<int64_t> convertToBox(const QRectF &input)
{
    return quadtree::Box<int64_t>(input.left(), input.top(), input.width(), input.height());
}

quadtree::Box<int64_t> computeSymbolBox(const QTransform &transform,
                                        const QPointF &pos,
                                        const TextureSymbol &textureSymbol)
{
    const auto topLeft = transform.map(pos) - textureSymbol.center;

    QRectF box(topLeft + textureSymbol.roi.topLeft(),
               textureSymbol.roi.size());
    return convertToBox(box);
}

// The root box of quadtree needs to be power of two so that the size of children
// are divisible by two. The assert in Quadtree.h: assert(box.contains(mGetBox(value)))
// fails for odd sized root boxes. Probably a bug in Quadtree.h.
QRect roundRectToPowersOfTwo(QRect rect)
{
    assert(rect.isValid());

    int nextWidth = pow(2, ceil(log(rect.width()) / log(2)));
    int nextHeight = pow(2, ceil(log(rect.height()) / log(2)));
    rect.setWidth(nextWidth);
    rect.setHeight(nextHeight);
    return rect;
}

quadtree::Box<int64_t> getDecentZoomedRegion(QTransform transform, const QRect &region)
{
    QRect zoomedRegion = transform.mapRect(region);

    QPoint originalCenter = zoomedRegion.center();

    // Quadtree needs a limited working box. A too large box will make the algorithm
    // slower. This number tells how many pixels outside of the tile we should care for
    // But because we further down in this function will round up to the closest
    // power of two width/height, this number will not have much to say.
    constexpr int pixelMargin = 50;

    constexpr QMargins margins(pixelMargin, pixelMargin, pixelMargin, pixelMargin);
    zoomedRegion = zoomedRegion.marginsAdded(margins);

    zoomedRegion = roundRectToPowersOfTwo(zoomedRegion);
    zoomedRegion.moveCenter(originalCenter);
    return convertToBox(zoomedRegion);
}
}

ZoomSweeper::ZoomSweeper(float maxZoom, const QRect &region)
    : m_region(region)
{
    int i = 0;

    for (const auto &zoomExp : m_zoomRatios) {
        const float zoom = maxZoom / pow(2, zoomExp / (static_cast<float>(std::size(m_zoomRatios)) - 1));

        QTransform transform;
        transform.scale(zoom, zoom);
        m_testZooms[i].transform = transform;
        m_testZooms[i].zoom = zoom;
        i++;
    }
}

void ZoomSweeper::calcSymbols(vector<AnnotationSymbol> &symbols)
{
    struct SymbolBox
    {
        quadtree::Box<int64_t> box;
        TextureSymbol symbol;
    };

    vector<AnnotationSymbol *> sortedSymbols;
    for (auto &symbol : symbols) {
        sortedSymbols.push_back(&symbol);
    }

    sort(sortedSymbols.begin(), sortedSymbols.end(), [](const AnnotationSymbol *a, const AnnotationSymbol *b) {
        return a->priority > b->priority;
    });

    // Place symbols
    for (const TestZoom &testZoom : m_testZooms) {
        const QTransform &transform = testZoom.transform;
        const float zoom = testZoom.zoom;

        auto regionBox = getDecentZoomedRegion(transform, m_region);

        auto getBox = [](const SymbolBox &node) {
            return node.box;
        };

        auto isEqual = [](const SymbolBox &a, const SymbolBox &b) -> bool {
            return a.box.left == b.box.left
                && a.box.top == b.box.top
                && a.box.width == b.box.width
                && a.box.height == b.box.height;
        };

        auto quadtree = quadtree::Quadtree<SymbolBox,
                                           decltype(getBox),
                                           decltype(isEqual),
                                           int64_t>(regionBox, getBox);

        // Create collision rectangles for symbols already shown at smaller zoom
        for (const auto &symbol : symbols) {
            if (symbol.symbol.has_value() && symbol.minZoom.has_value()) {
                auto box = computeSymbolBox(transform,
                                            symbol.pos,
                                            symbol.symbol.value());

                if (!regionBox.contains(box)) {
                    continue;
                }

                quadtree.add(SymbolBox { box, symbol.symbol.value() });
            }
        }

        for (auto *annotationSymbol : sortedSymbols) {
            if (!annotationSymbol->symbol.has_value()) {
                // This annotation has no actual symbol so we set minimum zoom
                // to any zoom so that child labels can be shown. This is done
                // to show label(s) for annotations without a symbol.
                annotationSymbol->minZoom = 0;
                continue;
            }

            if (annotationSymbol->minZoom.has_value()) {
                // This annotation was already set to be shown at smaller zoom
                continue;
            }

            auto box = computeSymbolBox(transform,
                                        annotationSymbol->pos,
                                        annotationSymbol->symbol.value());

            if (!regionBox.contains(box)) {
                continue;
            }

            if (annotationSymbol->collisionRule == CollisionRule::NoCheck) {
                continue;
            }

            SymbolBox symbolBox { box, annotationSymbol->symbol.value() };
            std::vector<SymbolBox> intersections = quadtree.query(symbolBox.box);

            if (intersections.empty()) {
                annotationSymbol->minZoom = zoom;
                quadtree.add(symbolBox);
            }
        }
    }
}

void ZoomSweeper::calcLabels(const vector<AnnotationSymbol> &symbols,
                             vector<AnnotationLabel> &labels)
{
    for (const TestZoom &testZoom : m_testZooms) {
        const QTransform &transform = testZoom.transform;
        const float zoom = testZoom.zoom;

        auto zoomedRegion = getDecentZoomedRegion(transform, m_region);

        auto getBox = [](const quadtree::Box<int64_t> &box) {
            return box;
        };

        auto isEqual = [](const quadtree::Box<int64_t> &a, const quadtree::Box<int64_t> &b) -> bool {
            return a.left == b.left
                && a.top == b.top
                && a.width == b.width
                && a.height == b.height;
        };

        auto quadtree = quadtree::Quadtree<quadtree::Box<int64_t>,
                                           decltype(getBox),
                                           decltype(isEqual),
                                           int64_t>(zoomedRegion, getBox, isEqual);

        for (auto &symbol : symbols) {
            if (symbol.symbol.has_value() && symbol.minZoom.has_value()) {
                const auto pos = transform.map(symbol.pos) - symbol.symbol.value().center;
                auto box = convertToBox(QRectF(pos, symbol.symbol.value().size));
                if (!zoomedRegion.contains(box)) {
                    continue;
                }
                quadtree.add(box);
            }
        }

        for (auto &label : labels) {
            if (label.minZoom.has_value()) {
                const auto pos = transform.map(label.pos) + label.offset;
                auto box = convertToBox(QRectF(pos, label.boundingBox.size()));
                if (!zoomedRegion.contains(box)) {
                    continue;
                }
                quadtree.add(box);
            }
        }

        for (auto &label : labels) {
            const AnnotationSymbol &parentSymbol = symbols[label.parentSymbolIndex];

            if (!parentSymbol.minZoom.has_value() || zoom < parentSymbol.minZoom.value()) {
                continue;
            }

            const auto pos = transform.map(label.pos) + label.offset;
            QRectF labelBox(pos, label.boundingBox.size());

            if (!labelBox.isValid()) {
                continue;
            }

            auto box = convertToBox(labelBox);

            if (!zoomedRegion.contains(box)) {
                continue;
            }

            if (quadtree.query(box).empty()) {
                label.minZoom = zoom;
                quadtree.add(box);
            }
        }
    }
}
