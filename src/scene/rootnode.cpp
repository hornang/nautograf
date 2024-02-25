#include <QQuickWindow>
#include <QSGTexture>

#include "annotation/annotationmaterial.h"
#include "line/linematerial.h"
#include "materialcreator.h"
#include "polygon/polygonmaterial.h"
#include "rootnode.h"
#include "symbolimage.h"

RootNode::RootNode(const QImage &symbolImage,
                   const QImage &fontImage,
                   const QQuickWindow *window)
{
    QSGTexture *symbolTexture = window->createTextureFromImage(symbolImage);
    symbolTexture->setFiltering(QSGTexture::Linear);

    QSGTexture *fontTexture = window->createTextureFromImage(fontImage);
    fontTexture->setFiltering(QSGTexture::Linear);

    m_materialCreator = new MaterialCreator(symbolTexture, fontTexture);
}

MaterialCreator *RootNode::materialCreator() const
{
    return m_materialCreator;
}

RootNode::~RootNode()
{
    if (m_materialCreator) {
        delete m_materialCreator;
    }
}
