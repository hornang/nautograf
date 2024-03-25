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

    void setWidth(float width)
    {
        uniforms.width = width;
        uniforms.dirty = true;
    }

    struct Uniforms
    {
        float width = 0;
        bool dirty = false;
    };

    Uniforms uniforms;
};
