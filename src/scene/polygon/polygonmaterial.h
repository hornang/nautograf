#pragma once

#include <QSGMaterial>
#include <QSGTexture>

class PolygonMaterial : public QSGMaterial
{
public:
    enum class BlendMode {
        Opaque,
        Alpha
    };
    PolygonMaterial(BlendMode blendMode = BlendMode::Opaque);
    QSGMaterialType *type() const override;
    int compare(const QSGMaterial *other) const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;
};
