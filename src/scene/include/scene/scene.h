#pragma once

#include <memory>

#include <QHash>
#include <QSet>
#include <QVector3D>
#include <QtQuick>

#include "scene/tessellator.h"
#include "tilefactory/chart.h"
#include "tilefactory/georect.h"
#include "tilefactorywrapper.h"

class SymbolImage;
class FontImage;

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
    void tessellatorDone(const QString &tileId);

signals:
    void tileFactoryChanged();
    void viewportChanged();
    void tileModelChanged();

private:
    static std::optional<TileFactoryWrapper::TileRecipe> parseTileRef(const QVariantMap &tileRef);

    template <typename T>
    void removeStaleNodes(QSGNode *parent) const;

    template <typename T>
    void updateNodeData(const QString &tileId,
                        QSGNode *parent,
                        const QHash<QString, T *> &existingNodes,
                        std::function<QList<typename T::Vertex>(const Tessellator::TileData &tileData)> getter,
                        QSGMaterial *material);

    template <typename T>
    static QHash<QString, T *> currentNodes(const QSGNode *parent);
    void markChildrensDirtyMaterial(QSGNode *parent);
    void addTessellatorsFromModel(TileFactoryWrapper *tileFactory, int first, int last);
    void updateBox();
    void getData();
    void fetchAll();

    QHash<QString, std::shared_ptr<Tessellator>> m_tessellators;
    QSet<QString> m_tessellatorsWithPendingData;
    std::shared_ptr<SymbolImage> m_symbolImage;
    std::shared_ptr<FontImage> m_fontImage;
    QVector3D m_viewport;
    QRectF m_boundingRect;

    /// The viewport in mercator coordinates
    QRectF m_box;

    TileFactoryWrapper *m_tileFactory = nullptr;
    QAbstractListModel *m_tileModel = nullptr;
    bool m_tessellatorRemoved = false;
    float m_zoom = 1.0;
};
