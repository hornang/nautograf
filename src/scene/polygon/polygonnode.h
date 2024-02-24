#pragma once

#include <QSGGeometryNode>

class QSGMaterial;

class PolygonNode : public QSGGeometryNode
{
public:
    struct Vertex
    {
        float x;
        float y;
        float z;
        uchar red;
        uchar green;
        uchar blue;
        uchar alpha;
    };
    PolygonNode(QSGMaterial *material, const QList<PolygonNode::Vertex> &vertices);
    void updateVertices(const QList<Vertex> &vertices);
};
