#include "polygonmaterial.h"
#include "polygonshader.h"

QSGMaterialType *PolygonMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

int PolygonMaterial::compare(const QSGMaterial *o) const
{
    Q_ASSERT(o && type() == o->type());
    const auto *other = static_cast<const PolygonMaterial *>(o);
    return other == this ? 0 : 1; // ### TODO: compare state???  <- From custommaterial example
}

QSGMaterialShader *PolygonMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new PolygonShader();
}
