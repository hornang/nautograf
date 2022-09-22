#include <QSGTexture>

#include "annotationmaterial.h"
#include "annotationshader.h"

AnnotationMaterial::AnnotationMaterial(QSGTexture *texture)
    : m_texture(texture)
{
    setFlag(QSGMaterial::RequiresFullMatrix | QSGMaterial::Blending);
}

QSGMaterialType *AnnotationMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

int AnnotationMaterial::compare(const QSGMaterial *o) const
{
    Q_ASSERT(o && type() == o->type());
    const auto *other = static_cast<const AnnotationMaterial *>(o);
    return other == this ? 0 : 1; // ### TODO: compare state???  <- From custommaterial example
}

QSGMaterialShader *AnnotationMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new AnnotationShader();
}
