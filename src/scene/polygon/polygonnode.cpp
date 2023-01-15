#include <QSGMaterial>

#include "polygonmaterial.h"
#include "polygonnode.h"

static const QSGGeometry::Attribute attributes[2] = {
    QSGGeometry::Attribute::createWithAttributeType(0, 3,
                                                    QSGGeometry::FloatType,
                                                    QSGGeometry::PositionAttribute),
    QSGGeometry::Attribute::createWithAttributeType(1, 4,
                                                    QSGGeometry::UnsignedByteType,
                                                    QSGGeometry::ColorAttribute),
};

static const QSGGeometry::AttributeSet attributeSet = { 2, 16, attributes };
static_assert(sizeof(PolygonNode::Vertex) == 16, "Incorrect sizeof(PolygonNode::Vertex)");

PolygonNode::PolygonNode(const QString &id,
                         QSGMaterial *material,
                         const QList<PolygonNode::Vertex> &vertices)
    : m_id(id)
{
    setMaterial(material);
    QSGGeometry *geometry = new QSGGeometry(attributeSet, vertices.length());
    memcpy(geometry->vertexData(), vertices.constData(), vertices.length() * sizeof(Vertex));
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    geometry->setVertexDataPattern(QSGGeometry::StaticPattern);
    setGeometry(geometry);

    setFlag(OwnsGeometry, true);
    setFlag(OwnedByParent, true);
}

void PolygonNode::updateVertices(const QList<PolygonNode::Vertex> &vertices)
{
    geometry()->allocate(vertices.length());
    memcpy(geometry()->vertexData(), vertices.constData(), vertices.length() * sizeof(Vertex));
    markDirty(DirtyGeometry);
}
