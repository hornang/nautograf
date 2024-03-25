#include "materialcreator.h"
#include "annotations/annotationmaterial.h"
#include "line/linematerial.h"
#include "polygon/polygonmaterial.h"
#include "tiledata.h"

MaterialCreator::MaterialCreator(QSGTexture *symbolTexture,
                                 QSGTexture *fontTexture)
    : m_symbolTexture(symbolTexture)
    , m_fontTexture(fontTexture)
{
}

void MaterialCreator::setZoom(float zoom)
{
    m_scale = zoom;

    if (m_symbolMaterial) {
        m_symbolMaterial->setZoom(zoom);
    }

    if (m_textMaterial) {
        m_textMaterial->setZoom(zoom);
    }
}

PolygonMaterial *MaterialCreator::opaquePolygonMaterial()
{
    if (!m_opaquePolygonMaterial) {
        m_opaquePolygonMaterial = std::make_unique<PolygonMaterial>(PolygonMaterial::BlendMode::Opaque);
    }

    return m_opaquePolygonMaterial.get();
}

PolygonMaterial *MaterialCreator::blendedPolygonMaterial()
{
    if (!m_blendedPolygonMaterial) {
        m_blendedPolygonMaterial = std::make_unique<PolygonMaterial>(PolygonMaterial::BlendMode::Alpha);
    }

    return m_blendedPolygonMaterial.get();
}

AnnotationMaterial *MaterialCreator::symbolMaterial()
{
    if (!m_symbolMaterial) {
        m_symbolMaterial = std::make_unique<AnnotationMaterial>(m_symbolTexture);
        m_symbolMaterial->setZoom(m_scale);
    }

    return m_symbolMaterial.get();
}

AnnotationMaterial *MaterialCreator::textMaterial()
{
    if (!m_textMaterial) {
        m_textMaterial = std::make_unique<AnnotationMaterial>(m_fontTexture);
        m_textMaterial->setZoom(m_scale);
    }

    return m_textMaterial.get();
}

using LineStyle = GeometryLayer::LineGroup::Style;

LineMaterial *MaterialCreator::lineMaterial(const LineStyle &style)
{
    if (!m_lineMaterials.contains(style)) {
        auto lineMaterial = std::make_unique<LineMaterial>();
        switch (style.width) {
        case LineStyle::Width::Thin:
            lineMaterial->setWidth(1.5);
            break;
        case LineStyle::Width::Medium:
            lineMaterial->setWidth(2.5);
            break;
        case LineStyle::Width::Thick:
            lineMaterial->setWidth(4);
            break;
        }
        m_lineMaterials[style] = std::move(lineMaterial);
    }

    return m_lineMaterials[style].get();
}
