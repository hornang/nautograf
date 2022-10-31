#pragma once

#include <QSGMaterial>
#include <QSGTexture>

class PolygonMaterial : public QSGMaterial
{
public:
    PolygonMaterial() = default;
    QSGMaterialType *type() const override;
    int compare(const QSGMaterial *other) const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;
};
