#pragma once

#include <map>
#include <memory>

class LineMaterial;
class PolygonMaterial;
class AnnotationMaterial;
class QSGTexture;

#include "tiledata.h"

class MaterialCreator
{
    using LineStyle = GeometryLayer::LineGroup::Style;

public:
    MaterialCreator() = delete;
    MaterialCreator(QSGTexture *symbolTexture, QSGTexture *fontTexture);
    void setZoom(float zoom);
    PolygonMaterial *opaquePolygonMaterial();
    PolygonMaterial *blendedPolygonMaterial();
    AnnotationMaterial *symbolMaterial();
    AnnotationMaterial *textMaterial();
    LineMaterial *lineMaterial(const LineStyle &style);

private:
    struct LineStyleHash
    {
        auto operator()(const LineStyle &p) const -> size_t
        {
            return std::hash<std::size_t> {}(size_t(p.width));
        }
    };

    float m_scale = 0;
    QSGTexture *m_symbolTexture = nullptr;
    QSGTexture *m_fontTexture = nullptr;
    std::unique_ptr<PolygonMaterial> m_opaquePolygonMaterial;
    std::unique_ptr<PolygonMaterial> m_blendedPolygonMaterial;
    std::unique_ptr<AnnotationMaterial> m_symbolMaterial;
    std::unique_ptr<AnnotationMaterial> m_textMaterial;
    std::unordered_map<LineStyle, std::unique_ptr<LineMaterial>, LineStyleHash> m_lineMaterials;
};
