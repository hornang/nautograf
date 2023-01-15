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
    PolygonNode(const QString &tileId,
                QSGMaterial *material,
                const QList<PolygonNode::Vertex> &vertices);
    const QString &id() const { return m_id; }
    void updateVertices(const QList<Vertex> &vertices);

private:
    QString m_id;
};
