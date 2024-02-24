#include "linenode.h"

static const QSGGeometry::Attribute attributes[] = {
    QSGGeometry::Attribute::createWithAttributeType(0, 3, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
    QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute),
    QSGGeometry::Attribute::createWithAttributeType(2, 4, QSGGeometry::UnsignedByteType, QSGGeometry::ColorAttribute),
};
static const QSGGeometry::AttributeSet attributeSet = { static_cast<int>(std::size(attributes)),
                                                        sizeof(LineNode::Vertex),
                                                        attributes };

static_assert(sizeof(LineNode::Vertex) == 24, "Incorrect sizeof(LineNode::Vertex)");

LineNode::LineNode(QSGMaterial *material,
                   const QList<LineNode::Vertex> &vertices)
{
    setMaterial(material);

    QSGGeometry *geometry = new QSGGeometry(attributeSet, vertices.length());
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    memcpy(geometry->vertexData(),
           vertices.constData(),
           vertices.length() * sizeof(LineNode::Vertex));

    setGeometry(geometry);
    setFlag(OwnsGeometry, true);
    setFlag(OwnedByParent, true);
}

void LineNode::updateVertices(const QList<LineNode::Vertex> &vertices)
{
    geometry()->allocate(vertices.length());
    memcpy(geometry()->vertexData(),
           vertices.constData(),
           vertices.length() * sizeof(LineNode::Vertex));
    markDirty(DirtyGeometry | DirtyMaterial);
}
