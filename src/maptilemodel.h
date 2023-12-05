#pragma once

#include "tilefactory/georect.h"
#include "tilefactory/pos.h"
#include "tilefactory/tilefactory.h"

#include <QAbstractListModel>
#include <QRect>

class MapTileModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QSizeF viewPort READ viewPort WRITE setViewPort NOTIFY viewPortChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        TileRefRole,
        TileIdRole,
        TopLatitude,
        LeftLongitude,
        BottomLatitude,
        RightLongitude,
    };

    MapTileModel(const std::shared_ptr<TileFactory> &tileFactory);
    QSizeF viewPort() const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QHash<int, QByteArray> roleNames() const;
    void chartsChanged(const std::vector<GeoRect> &rects);
    int count() const;
    void scheduleUpdate();

public slots:
    void setViewPort(const QSizeF &viewPort);
    void setPanZoom(double lon, double lat, double pixelsPerLon);
    QVariantMap tileRefAtPos(float lat, float lon);
    void update();

signals:
    void viewPortChanged(QSizeF viewPort);
    void countChanged(int count);

private:
    static QVariantMap createTileRef(const QString &tileId,
                                     const GeoRect &boundingBox,
                                     int maxPixelsPerLon);
    std::shared_ptr<TileFactory> m_tileFactory;
    Pos m_center;
    double m_pixelsPerLon = 50000;
    QHash<int, QByteArray> m_roleNames;
    QVector<TileFactory::Tile> m_tiles;
    QSizeF m_viewPort;
};
