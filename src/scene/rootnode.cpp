#include <QQuickWindow>
#include <QSGTexture>

#include "annotation/annotationmaterial.h"
#include "polygon/polygonmaterial.h"
#include "rootnode.h"
#include "scene/fontimage.h"
#include "scene/symbolimage.h"

RootNode::RootNode(const QImage &symbolImage,
                   const QImage &fontImage,
                   const QQuickWindow *window)
{
    m_symbolTexture = window->createTextureFromImage(symbolImage);
    m_symbolTexture->setFiltering(QSGTexture::Linear);

    m_fontTexture = window->createTextureFromImage(fontImage);
    m_fontTexture->setFiltering(QSGTexture::Linear);

    m_polygonMaterial = new PolygonMaterial();
    m_symbolMaterial = new AnnotationMaterial(m_symbolTexture);
    m_textMaterial = new AnnotationMaterial(m_fontTexture);
}

RootNode::~RootNode()
{
    if (m_symbolTexture) {
        delete m_symbolTexture;
    }

    if (m_fontTexture) {
        delete m_fontTexture;
    }

    delete m_symbolMaterial;
    delete m_textMaterial;
}
