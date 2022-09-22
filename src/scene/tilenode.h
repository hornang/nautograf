#pragma once

#include <QObject>
#include <QSGNode>
#include <QSGTexture>

#include "tessellator.h"

class TileFactoryWrapper;

class TileNode : public QObject, public QSGNode
{
    Q_OBJECT
public:
    struct Textures
    {
        QSGTexture *fontTexture;
        QSGTexture *symbolTexture;
    };

    TileNode(const QString &tileId,
             TileFactoryWrapper *tileFactory,
             const FontImage *fontImage,
             const SymbolImage *symbolImage,
             Textures textures,
             TileFactoryWrapper::TileRecipe recipe);

    void setTileFactory(TileFactoryWrapper *tileFactory);
    void fetchAgain();
    QString tileId() const { return m_tileId; }
    void updatePaintNode(float scale);
signals:
    void update();

private:
    QString m_tileId;
    Tessellator m_tesselator;
    QSGTexture *m_fontTexture = nullptr;
    QSGTexture *m_symbolTexture = nullptr;
};
