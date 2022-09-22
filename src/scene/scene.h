#pragma once

#include <QHash>
#include <QVector3D>
#include <QtQuick>

#include "fontimage.h"
#include "symbolimage.h"
#include "tessellator.h"
#include "tilefactory/chart.h"
#include "tilefactory/georect.h"

class TileFactoryWrapper;

class Scene : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(TileFactoryWrapper *tileFactory READ tileFactory WRITE setTileFactory NOTIFY tileFactoryChanged)
    Q_PROPERTY(QAbstractListModel *tileModel READ tileModel WRITE setModel NOTIFY tileModelChanged)

    /*!
        \property viewport

        The viewport is QVector3D where the x, y and z coordinates are (mis-)used
        as follows:
        - x longitude
        - y longitude
        - z pixelsPerLon
    */
    Q_PROPERTY(QVector3D viewport READ viewport WRITE setViewport NOTIFY viewportChanged)

public:
    Scene(QQuickItem *parent = 0);

    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *);
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry);
    TileFactoryWrapper *tileFactory() const;
    void setTileFactory(TileFactoryWrapper *newTileFactory);

    const QVector3D &viewport() const;
    void setViewport(const QVector3D &newViewport);

    QAbstractListModel *tileModel() const;
    void setModel(QAbstractListModel *newTileModel);

public slots:
    void rowsInserted(const QModelIndex &parent, int first, int last);
    void rowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void dataChanged(const QModelIndex &topLeft,
                     const QModelIndex &bottomRight,
                     const QList<int> &roles);

signals:
    void tileFactoryChanged();
    void viewportChanged();
    void tileModelChanged();

private:
    static std::optional<TileFactoryWrapper::TileRecipe> parseTileRef(const QVariantMap &tileRef);
    void updateBox();
    void getData();

    QHash<QString, TileFactoryWrapper::TileRecipe> m_tiles;
    SymbolImage m_symbolImage;
    FontImage m_fontImage;
    QVector3D m_viewport;
    QRectF m_boundingRect;

    /// The viewport in mercator coordinates
    QRectF m_box;

    TileFactoryWrapper *m_tileFactory = nullptr;
    QAbstractListModel *m_tileModel;
    bool m_sourceDataChanged = false;
    bool m_tilesChanged = false;
};
