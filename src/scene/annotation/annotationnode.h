#pragma once

#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGTexture>
#include <QSGMaterialShader>

class AnnotationNode : public QSGGeometryNode
{
public:
    AnnotationNode(QSGTexture *texture, const QList<float> &vertices, float scale);
    ~AnnotationNode();

public:
    void setPositions(const QList<QPointF> &positions);
    void setScale(qreal scale);

private:
    float m_scale = 1;
    QSGGeometry::Attribute m_attributes[5];
    QSGGeometry::AttributeSet m_attributeSet = { 5, 40, m_attributes };
    QSGTexture *m_texture = nullptr;
};
