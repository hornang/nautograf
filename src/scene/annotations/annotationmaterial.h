#pragma once

#include <QSGMaterial>
#include <QSGTexture>

class AnnotationMaterial : public QSGMaterial
{
public:
    AnnotationMaterial(QSGTexture *texture);
    QSGMaterialType *type() const override;

    int compare(const QSGMaterial *other) const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;
    QSGTexture *texture() const { return m_texture; }
    void setZoom(float zoom)
    {
        uniforms.zoom = zoom;
        uniforms.dirty = true;
    }

    struct
    {
        float zoom = 0;
        bool dirty = false;
    } uniforms;

private:
    QSGTexture *m_texture = nullptr;
};
