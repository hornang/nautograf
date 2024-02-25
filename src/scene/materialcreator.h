#pragma once

#include <memory>

class LineMaterial;
class PolygonMaterial;
class AnnotationMaterial;
class QSGTexture;

class MaterialCreator
{

public:
    MaterialCreator() = delete;
    MaterialCreator(QSGTexture *symbolTexture, QSGTexture *fontTexture);
    void setScale(float scale);
    PolygonMaterial *opaquePolygonMaterial();
    PolygonMaterial *blendedPolygonMaterial();
    AnnotationMaterial *symbolMaterial();
    AnnotationMaterial *textMaterial();
    LineMaterial *lineMaterial();

private:
    QSGTexture *m_symbolTexture = nullptr;
    QSGTexture *m_fontTexture = nullptr;
    std::unique_ptr<PolygonMaterial> m_opaquePolygonMaterial;
    std::unique_ptr<PolygonMaterial> m_blendedPolygonMaterial;
    std::unique_ptr<AnnotationMaterial> m_symbolMaterial;
    std::unique_ptr<AnnotationMaterial> m_textMaterial;
    std::unique_ptr<LineMaterial> m_lineMaterial;
};
