#include <QList>
#include <QPointF>
#include <QRectF>
#include <QTransform>

#include "annotater.h"
#include "zoomsweeper.h"

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
        const float zoom = maxZoom / pow(2, zoomExp / (std::size(m_zoomRatios) - 1));

        QTransform transform;
        transform.scale(zoom, zoom);
        m_transforms[i++] = transform;
    }
}

void ZoomSweeper::calcAnnotations(QList<Annotation> &annotations)
{
    struct SymbolBox
    {
        QRectF box;
        TextureSymbol symbol;
    };

    // Place symbols
    for (const auto &transform : m_transforms) {
        const auto zoom = transform.m11();

        QList<SymbolBox> existingBoxes;

        // Create collision rectangles for symbols already shown at smaller zoom
        for (const auto &annotation : annotations) {
            if (annotation.symbol.has_value() && annotation.minZoom.has_value()) {
                existingBoxes.append(SymbolBox { computeSymbolRect(transform,
                                                                   annotation.pos,
                                                                   annotation.symbol.value()),
                                                 annotation.symbol.value() });
            }
        }

        for (auto &annotation : annotations) {
            if (!annotation.symbol.has_value()) {
                // This annotation has no actual symbol so we set minimum zoom
                // to any zoom so that child labels can be shown. This is done
                // to show label(s) for annotations without a symbol.
                annotation.minZoom = 0;
                continue;
            }

            if (annotation.minZoom.has_value()) {
                // This annotation was already set to be shown at smaller zoom
                continue;
            }

            SymbolBox symbolBox { computeSymbolRect(transform,
                                                    annotation.pos,
                                                    annotation.symbol.value()),
                                  annotation.symbol.value() };

            bool collision = false;

            if (annotation.collisionRule != CollisionRule::NoCheck) {
                for (const auto &other : existingBoxes) {
                    if (symbolBox.box.intersects(other.box)
                        && (annotation.collisionRule == CollisionRule::Always || other.symbol == symbolBox.symbol)) {
                        collision = true;
                        break;
                    }
                }
            }

            if (!collision) {
                annotation.minZoom = zoom;
                existingBoxes.append(symbolBox);
            }
        }
    }
}

QList<AnnotationLabel> ZoomSweeper::calcLabels(const QList<Annotation> &annotations)
{
    QList<AnnotationLabel> labels;

    for (const Annotation &annotation : annotations) {
        for (const AnnotationLabel &label : annotation.labels) {
            auto it = labels.emplace(labels.cend(), label);
            it->parentMinZoom = annotation.minZoom;
        }
    }

    for (const auto &transform : m_transforms) {
        const auto zoom = transform.m11();

        QList<QRectF> boxes;

        for (auto &annotation : annotations) {
            if (annotation.symbol.has_value() && annotation.minZoom.has_value()) {
                const auto pos = transform.map(annotation.pos) - annotation.symbol.value().center;
                boxes.append(QRectF(pos, annotation.symbol.value().size));
            }
        }

        for (auto &label : labels) {
            if (label.minZoom.has_value()) {
                const auto pos = transform.map(label.pos) + label.offset;
                boxes.append(QRectF(pos, label.boundingBox.size()));
            }
        }

        for (auto &label : labels) {
            if (!label.parentMinZoom.has_value() || zoom < label.parentMinZoom.value()) {
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

    return labels;
}
