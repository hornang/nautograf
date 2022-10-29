#include <QElapsedTimer>
#include <QObject>
#include <QSGSimpleRectNode>

#include "annotation/annotationnode.h"
#include "tilefactory/mercator.h"
#include "tilenode.h"

TileNode::TileNode(const QString &tileId,
                   TileFactoryWrapper *tileFactory,
                   const FontImage *fontImage,
                   const SymbolImage *symbolImage,
                   Textures textures,
                   TileFactoryWrapper::TileRecipe recipe)
    : m_fontTexture(textures.fontTexture)
    , m_symbolTexture(textures.symbolTexture)
    , m_tileId(tileId)
    , m_tesselator(tileFactory, recipe, symbolImage, fontImage)
{
    m_tesselator.fetchAgain();
    QObject::connect(&m_tesselator, &Tessellator::dataChanged, this, [&]() {
        emit update();
    });

    setFlag(OwnedByParent, true);
}

void TileNode::fetchAgain()
{
    m_tesselator.fetchAgain();
}

void TileNode::setTileFactory(TileFactoryWrapper *tileFactory)
{
    m_tesselator.setTileFactory(tileFactory);
}

void TileNode::updatePaintNode(float scale)
{
    if (m_tesselator.hasDataChanged()) {
        m_tesselator.resetDataChanged();

        auto *child = firstChild();

        while (child) {
            QSGNode * next = child->nextSibling();
            delete child;
            child = next;
        }

        removeAllChildNodes();
        const auto tileData = m_tesselator.data();

        appendChildNode(new AnnotationNode(m_symbolTexture,
                                           tileData.symbolVertices,
                                           scale));

        appendChildNode(new AnnotationNode(m_fontTexture,
                                           tileData.textVertices,
                                           scale));
    }

    auto *child = firstChild();

    while (child) {
        auto *annotationNode = static_cast<AnnotationNode *>(child);
        annotationNode->setScale(scale);
        child = child->nextSibling();
    }
}
