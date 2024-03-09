#pragma once

#include "annotations/annotationnode.h"
#include "line/linenode.h"
#include "polygon/polygonnode.h"

struct GeometryLayer
{
    QList<PolygonNode::Vertex> polygonVertices;
    QList<LineNode::Vertex> lineVertices;
};

struct TileData
{
    // Per layer (chart) geometric objects
    QList<GeometryLayer> geometryLayers;

    // Symbolism and text for all layers
    QList<AnnotationNode::Vertex> symbolVertices;
    QList<AnnotationNode::Vertex> textVertices;
};
