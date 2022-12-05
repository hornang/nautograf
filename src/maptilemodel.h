#pragma once

#include "tilefactory/georect.h"
#include "tilefactory/pos.h"
#include "tilefactory/tilefactory.h"

#include <QAbstractListModel>
#include <QRect>

class MapTileModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QPointF topLeft READ topLeft WRITE setTopLeft NOTIFY topLeftChanged)
    Q_PROPERTY(QSizeF viewPort READ viewPort WRITE setViewPort NOTIFY viewPortChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(qreal pixelsPerLongitude READ pixelsPerLongitude WRITE setPixelsPerLongitude NOTIFY pixelsPerLongitudeChanged)

public:
    enum Role {
        TileRefRole,
        TileIdRole,
        TopLatitude,
        LeftLongitude,
        BottomLatitude,
        RightLongitude,
    };

    MapTileModel(const std::shared_ptr<TileFactory> &tileManager);
    QPointF topLeft() const;
    void setTopLeft(const QPointF &topLeft);
    qreal pixelsPerLongitude() const;
    void setPixelsPerLongitude(qreal pixelsPerLongitude);
    QSizeF viewPort() const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QHash<int, QByteArray> roleNames() const;
    void chartsChanged(const std::vector<GeoRect> &rects);
    int count() const;
    void scheduleUpdate();

public slots:
    void setViewPort(const QSizeF &viewPort);
    void setPanZoom(QPointF topLeft, qreal pixelsPerLongitude);
    void update();

signals:
    void topLeftChanged(const QPointF & topLeft);
    void pixelsPerLongitudeChanged(qreal pixelsPerLongitude);
    void viewPortChanged(QSizeF viewPort);
    void countChanged(int count);

private:
    void reset();
    std::shared_ptr<TileFactory> m_tileManager;
    Pos m_topLeft;
    int m_pixelsPerLongitude = 50000;
    QHash<int, QByteArray> m_roleNames;
    QVector<TileFactory::Tile> m_tiles;
    QSizeF m_viewPort;
};
