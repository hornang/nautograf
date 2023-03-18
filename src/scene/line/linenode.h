#pragma once

#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>
#include <QString>

class LineMaterial;

class LineNode : public QSGGeometryNode
{
public:
    struct Vertex
    {
        float xPos;
        float yPos;
        float zPos;
        float xOffset;
        float yOffset;
        uchar red;
        uchar green;
        uchar blue;
        uchar alpha;
    };
    LineNode(const QString &id,
             QSGMaterial *material,
             const QList<LineNode::Vertex> &vertices);
    const QString &id() const { return m_id; }

public:
    void updateVertices(const QList<LineNode::Vertex> &vertices);

private:
    QString m_id;
};
