#pragma once

#include "annotation/annotationnode.h"
#include "polygon/polygonnode.h"

struct GeometryLayer
{
    QList<PolygonNode::Vertex> polygonVertices;
};

struct TileData
{
    // Per layer (chart) geometric objects
    QList<GeometryLayer> geometryLayers;

    // Symbolism and text for all layers
    QList<AnnotationNode::Vertex> symbolVertices;
    QList<AnnotationNode::Vertex> textVertices;
};
