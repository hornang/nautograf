#include <QSGTexture>

#include "linematerial.h"
#include "lineshader.h"

LineMaterial::LineMaterial()
{
    setFlag(QSGMaterial::Blending);
}

QSGMaterialType *LineMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

int LineMaterial::compare(const QSGMaterial *o) const
{
    Q_ASSERT(o && type() == o->type());
    const auto *other = static_cast<const LineMaterial *>(o);
    return other == this ? 0 : 1; // ### TODO: compare state???  <- From custommaterial example
}

QSGMaterialShader *LineMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new LineShader();
}
