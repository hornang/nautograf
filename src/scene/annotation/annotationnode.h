#pragma once

#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>
#include <QString>

class AnnotationMaterial;

class AnnotationNode : public QSGGeometryNode
{
public:
    struct Vertex
    {
        float xCenter;
        float yCenter;
        float xOffset;
        float yOffset;
        float xTexture;
        float yTexture;
        float red;
        float green;
        float blue;
        float scaleLimit;
    };
    AnnotationNode(const QString &tileId,
                   QSGMaterial *material,
                   const QList<AnnotationNode::Vertex> &vertices);
    ~AnnotationNode();
    const QString &tileId() const { return m_tileId; }

public:
    void updateVertices(const QList<AnnotationNode::Vertex> &vertices);

private:
    QString m_tileId;
};
