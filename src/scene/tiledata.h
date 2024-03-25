#pragma once

#include "annotations/annotationnode.h"
#include "line/linenode.h"
#include "polygon/polygonnode.h"

struct GeometryLayer
{
    struct LineGroup
    {
        struct Style
        {
            bool operator==(const Style &other) const
            {
                return width == other.width;
            }

            enum class Width {
                Thin,
                Medium,
                Thick,
            };

            Width width = Width::Medium;
        };

        QList<LineNode::Vertex> vertices;
        Style style = { Style::Width::Medium };
    };

    QList<PolygonNode::Vertex> polygonVertices;
    QList<LineGroup> lineGroups;
};

struct TileData
{
    // Per layer (chart) geometric objects
    QList<GeometryLayer> geometryLayers;

    // Symbolism and text for all layers
    QList<AnnotationNode::Vertex> symbolVertices;
    QList<AnnotationNode::Vertex> textVertices;
};
