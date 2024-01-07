#pragma once

#include <memory>

#include <QHash>
#include <QSet>
#include <QtQuick>

#include "tilefactory/chart.h"
#include "tilefactory/georect.h"
#include "tilefactorywrapper.h"

#include "scene_export.h"

class SymbolImage;
class FontImage;
class Tessellator;
class TileData;

class SCENE_EXPORT Scene : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(TileFactoryWrapper *tileFactory READ tileFactory WRITE setTileFactory NOTIFY tileFactoryChanged)
    Q_PROPERTY(QAbstractListModel *tileModel READ tileModel WRITE setModel NOTIFY tileModelChanged)
    Q_PROPERTY(double lat READ lat WRITE setLat NOTIFY latChanged)
    Q_PROPERTY(double lon READ lon WRITE setLon NOTIFY lonChanged)
    Q_PROPERTY(double pixelsPerLon READ pixelsPerLon WRITE setPixelsPerLon NOTIFY pixelsPerLonChanged)
    Q_PROPERTY(QString focusedTile READ focusedTile WRITE setFocusedTile NOTIFY setFocusedTileChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)

public:
    Scene(QQuickItem *parent = 0);

    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *);
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry);
    TileFactoryWrapper *tileFactory() const;
    void setTileFactory(TileFactoryWrapper *newTileFactory);

    double lat() const;
    void setLat(double newLat);

    double lon() const;
    void setLon(double newLon);

    double pixelsPerLon() const;
    void setPixelsPerLon(double newPixelsPerLon);

    QAbstractListModel *tileModel() const;
    void setModel(QAbstractListModel *newTileModel);

    static std::optional<TileFactoryWrapper::TileRecipe> parseTileRef(const QVariantMap &tileRef);
    const QString &focusedTile() const;
    void setFocusedTile(const QString &newFocusedTile);

    const QColor &accentColor() const;
    void setAccentColor(const QColor &newAccentColor);

public slots:
    void rowsInserted(const QModelIndex &parent, int first, int last);
    void rowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void dataChanged(const QModelIndex &topLeft,
                     const QModelIndex &bottomRight,
                     const QList<int> &roles);
    void tessellatorDone(const QString &tileId);
    void tileDataChanged(const QStringList &tileIds);

signals:
    void tileFactoryChanged();
    void latChanged();
    void lonChanged();
    void pixelsPerLonChanged();
    void tileModelChanged();
    void setFocusedTileChanged();

    void accentColorChanged();

private:
    template <typename T>
    void removeStaleNodes(QSGNode *parent) const;
    void markChildrensDirtyMaterial(QSGNode *parent);
    void addTessellatorsFromModel(TileFactoryWrapper *tileFactory, int first, int last);
    void updateBox();
    void getData();
    void fetchAll();

    QHash<QString, std::shared_ptr<Tessellator>> m_tessellators;
    QSet<QString> m_newTessellators;
    QSet<QString> m_tessellatorsWithPendingData;
    std::shared_ptr<SymbolImage> m_symbolImage;
    std::shared_ptr<FontImage> m_fontImage;

    double m_lat = 0;
    double m_lon = 0;
    double m_pixelsPerLon = 300;
    QRectF m_boundingRect;

    /// The viewport in mercator coordinates
    QRectF m_box;

    QString m_focusedTile;
    bool m_focusedTileChanged = false;

    TileFactoryWrapper *m_tileFactory = nullptr;
    QAbstractListModel *m_tileModel = nullptr;
    bool m_tessellatorRemoved = false;
    float m_zoom = 1.0;
    QColor m_accentColor;
    QColor m_overlayColor;
};
