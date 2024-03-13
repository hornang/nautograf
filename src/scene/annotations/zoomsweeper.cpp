#include <QList>
#include <QPointF>
#include <QRectF>
#include <QTransform>

#include "annotater.h"
#include "zoomsweeper.h"

using namespace std;

namespace {
QRectF computeSymbolRect(const QTransform &transform,
                         const QPointF &pos,
                         const TextureSymbol &textureSymbol)
{
    const auto topLeft = transform.map(pos) - textureSymbol.center;
    return QRectF(topLeft + textureSymbol.roi.topLeft(),
                  textureSymbol.roi.size());
}
}

ZoomSweeper::ZoomSweeper(float maxZoom)
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
        QRectF box;
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

        QList<SymbolBox> existingBoxes;

        // Create collision rectangles for symbols already shown at smaller zoom
        for (const auto &symbol : symbols) {
            if (symbol.symbol.has_value() && symbol.minZoom.has_value()) {
                existingBoxes.append(SymbolBox { computeSymbolRect(transform,
                                                                   symbol.pos,
                                                                   symbol.symbol.value()),
                                                 symbol.symbol.value() });
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

            SymbolBox symbolBox { computeSymbolRect(transform,
                                                    annotationSymbol->pos,
                                                    annotationSymbol->symbol.value()),
                                  annotationSymbol->symbol.value() };

            bool collision = false;

            if (annotationSymbol->collisionRule != CollisionRule::NoCheck) {
                for (const auto &other : existingBoxes) {
                    if (symbolBox.box.intersects(other.box)
                        && (annotationSymbol->collisionRule == CollisionRule::Always || other.symbol == symbolBox.symbol)) {
                        collision = true;
                        break;
                    }
                }
            }

            if (!collision) {
                annotationSymbol->minZoom = zoom;
                existingBoxes.append(symbolBox);
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

        QList<QRectF> boxes;

        for (auto &symbol : symbols) {
            if (symbol.symbol.has_value() && symbol.minZoom.has_value()) {
                const auto pos = transform.map(symbol.pos) - symbol.symbol.value().center;
                boxes.append(QRectF(pos, symbol.symbol.value().size));
            }
        }

        for (auto &label : labels) {
            if (label.minZoom.has_value()) {
                const auto pos = transform.map(label.pos) + label.offset;
                boxes.append(QRectF(pos, label.boundingBox.size()));
            }
        }

        for (auto &label : labels) {
            const AnnotationSymbol &parentSymbol = symbols[label.parentSymbolIndex];

            if (!parentSymbol.minZoom.has_value() || zoom < parentSymbol.minZoom.value()) {
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
                label.minZoom = zoom;
                boxes.append(labelBox);
            }
        }
    }
}
