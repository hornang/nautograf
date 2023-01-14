#pragma once

#include "annotation/annotationnode.h"
#include "polygon/polygonnode.h"

struct TileData
{
    QList<AnnotationNode::Vertex> symbolVertices;
    QList<AnnotationNode::Vertex> textVertices;
    QList<PolygonNode::Vertex> polygonVertices;
};
