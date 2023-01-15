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
        uchar red;
        uchar green;
        uchar blue;
        uchar alpha; // For padding purposes. Ignored by shader
        float scaleLimit;
    };
    AnnotationNode(const QString &tileId,
                   QSGMaterial *material,
                   const QList<AnnotationNode::Vertex> &vertices);
    ~AnnotationNode();
    const QString &id() const { return m_id; }

public:
    void updateVertices(const QList<AnnotationNode::Vertex> &vertices);

private:
    QString m_id;
};
