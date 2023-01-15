#include "annotationnode.h"

static const QSGGeometry::Attribute attributes[] = {
    QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
    QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute),
    QSGGeometry::Attribute::createWithAttributeType(2, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
    QSGGeometry::Attribute::createWithAttributeType(3, 4, QSGGeometry::UnsignedByteType, QSGGeometry::ColorAttribute),
    QSGGeometry::Attribute::createWithAttributeType(4, 1, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute)
};
static const QSGGeometry::AttributeSet attributeSet = { static_cast<int>(std::size(attributes)),
                                                        sizeof(AnnotationNode::Vertex),
                                                        attributes };

static_assert(sizeof(AnnotationNode::Vertex) == 32, "Incorrect sizeof(AnnotationNode::Vertex)");

AnnotationNode::AnnotationNode(const QString &id,
                               QSGMaterial *material,
                               const QList<AnnotationNode::Vertex> &vertices)
    : m_id(id)
{
    setMaterial(material);

    QSGGeometry *geometry = new QSGGeometry(attributeSet, vertices.length());
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    memcpy(geometry->vertexData(),
           vertices.constData(),
           vertices.length() * sizeof(AnnotationNode::Vertex));

    setGeometry(geometry);
    setFlag(OwnsGeometry, true);
    setFlag(OwnedByParent, true);
}

AnnotationNode::~AnnotationNode()
{
    qDebug() << "~AnnotationNode";
}

void AnnotationNode::updateVertices(const QList<AnnotationNode::Vertex> &vertices)
{
    geometry()->allocate(vertices.length());
    memcpy(geometry()->vertexData(),
           vertices.constData(),
           vertices.length() * sizeof(AnnotationNode::Vertex));
    markDirty(DirtyGeometry | DirtyMaterial);
}
