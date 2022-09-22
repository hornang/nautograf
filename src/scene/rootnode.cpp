#include <QQuickWindow>
#include <QSGTexture>

#include "fontimage.h"
#include "rootnode.h"
#include "symbolimage.h"

RootNode::RootNode(const SymbolImage &symbolImage,
                   const FontImage &fontImage,
                   QQuickWindow *window)
{
    m_symbolTexture = window->createTextureFromImage(symbolImage.image());
    m_symbolTexture->setFiltering(QSGTexture::Linear);

    m_fontTexture = window->createTextureFromImage(fontImage.image());
    m_fontTexture->setFiltering(QSGTexture::Linear);
}

RootNode::~RootNode()
{
    if (m_symbolTexture) {
        delete m_symbolTexture;
    }

    if (m_fontTexture) {
        delete m_fontTexture;
    }
}
