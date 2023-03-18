#pragma once

#include <QSGMaterial>
#include <QSGTexture>

class LineMaterial : public QSGMaterial
{
public:
    LineMaterial();
    QSGMaterialType *type() const override;

    int compare(const QSGMaterial *other) const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;
};
