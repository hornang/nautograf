#include "annotationnode.h"
#include "annotationmaterial.h"

AnnotationNode::AnnotationNode(QSGTexture *texture,
                               const QList<float> &vertices,
                               float scale)
    : m_texture(texture)
    , m_scale(scale)
{
    auto *material = new AnnotationMaterial(texture);
    setMaterial(material);
    setFlag(OwnsMaterial, true);

    m_attributes[0] = QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute);
    m_attributes[1] = QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute);
    m_attributes[2] = QSGGeometry::Attribute::createWithAttributeType(2, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute);
    m_attributes[3] = QSGGeometry::Attribute::createWithAttributeType(3, 3, QSGGeometry::FloatType, QSGGeometry::ColorAttribute);
    m_attributes[4] = QSGGeometry::Attribute::createWithAttributeType(4, 1, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute);

    Q_ASSERT(vertices.length() % 10 == 0);
    QSGGeometry *geometry = new QSGGeometry(m_attributeSet, vertices.length() / 10);

    float *data = static_cast<float *>(geometry->vertexData());
    memcpy(data, static_cast<const float *>(vertices.constData()), vertices.length() * 4);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);

    setGeometry(geometry);
    setFlag(OwnsGeometry, true);
    setFlag(OwnedByParent, true);

    setScale(scale);
}

AnnotationNode::~AnnotationNode()
{
    qDebug() << "~AnnotationNode";
}

void AnnotationNode::setScale(qreal scale)
{
    if (m_scale == scale && false) {
        return;
    }
    m_scale = scale;
    auto *m = static_cast<AnnotationMaterial *>(material());
    m->uniforms.scale = scale;
    m->uniforms.dirty = true;
    markDirty(DirtyMaterial);
}
