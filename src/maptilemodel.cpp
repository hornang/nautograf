#include <algorithm>

#include <QDebug>
// #include <xlocale>

#include "maptile.h"
#include "maptilemodel.h"
#include "tilefactory/mercator.h"

MapTileModel::MapTileModel(const std::shared_ptr<TileFactory> &tileFactory)
    : m_tileFactory(tileFactory)
{
    m_roleNames[Role::TileRefRole] = "tileRef";
    m_roleNames[Role::TileIdRole] = "tileId";
    m_roleNames[Role::TopLatitude] = "topLatitude";
    m_roleNames[Role::LeftLongitude] = "leftLongitude";
    m_roleNames[Role::BottomLatitude] = "bottomLatitude";
    m_roleNames[Role::RightLongitude] = "rightLongitude";
    update();
}

void MapTileModel::setPanZoom(double lon, double lat, double pixelsPerLon)
{
    Pos topLeft(lat, lon);

    if (m_center == topLeft && m_pixelsPerLon == pixelsPerLon) {
        return;
    }

    m_center = topLeft;
    m_pixelsPerLon = pixelsPerLon;

    update();
}

void MapTileModel::chartsChanged(const std::vector<GeoRect> &rects)
{

    QList<int> affectedTiles;

    int i = 0;
    for (const auto &tile : m_tiles) {
        for (const auto &rect : rects) {
            if (tile.boundingBox.intersects(rect)) {
                if (!affectedTiles.contains(i)) {
                    affectedTiles.append(i);
                }
            }
        }
        i++;
    }

    for (const auto &affectedTile : affectedTiles) {
        emit dataChanged(createIndex(affectedTile, 0), createIndex(affectedTile, 0));
    }
}

void MapTileModel::scheduleUpdate()
{
    QMetaObject::invokeMethod(this, &MapTileModel::update, Qt::QueuedConnection);
}

void MapTileModel::update()
{
    std::vector<TileFactory::Tile> tiles;

    if (m_viewPort.isValid()) {
        tiles = m_tileFactory->tiles(m_center, m_pixelsPerLon, m_viewPort.width(), m_viewPort.height());
    }

    QHash<std::string, TileFactory::Tile> correct;

    for (const TileFactory::Tile &tile : tiles) {
        correct[tile.tileId] = tile;
    }

    for (int i = 0; i < m_tiles.length(); i++) {
        if (!correct.contains(m_tiles[i].tileId)) {
            beginRemoveRows(QModelIndex(), i, i);
            m_tiles.remove(i);
            endRemoveRows();
            i--;
        }
    }

    for (const TileFactory::Tile &tile : m_tiles) {
        correct.remove(tile.tileId);
    }

    if (!correct.isEmpty()) {
        beginInsertRows(QModelIndex(), m_tiles.size(), m_tiles.size() + correct.size() - 1);
        QList<std::string> keys = correct.keys();
        for (const std::string &key : keys) {
            TileFactory::Tile tile = correct[key];
            m_tiles.append(tile);
        }
        endInsertRows();
    }
    emit countChanged(count());
}

int MapTileModel::count() const
{
    return m_tiles.size();
}

void MapTileModel::setViewPort(const QSizeF &viewPort)
{
    if (m_viewPort == viewPort)
        return;

    m_viewPort = viewPort;
    emit viewPortChanged(m_viewPort);
    update();
}

int MapTileModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return m_tiles.length();
    }
    return 0;
}

QVariantMap MapTileModel::createTileRef(const QString &tileId,
                                        const GeoRect &boundingBox,
                                        int maxPixelsPerLon)
{
    QVariantMap tileRef;
    tileRef["tileId"] = tileId;
    tileRef["topLatitude"] = boundingBox.top();
    tileRef["leftLongitude"] = boundingBox.left();
    tileRef["bottomLatitude"] = boundingBox.bottom();
    tileRef["rightLongitude"] = boundingBox.right();
    tileRef["maxPixelsPerLongitude"] = maxPixelsPerLon;
    return tileRef;
}

QVariant MapTileModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();

    if (row < 0 || row > m_tiles.size() - 1) {
        return QVariant();
    }

    const TileFactory::Tile &tile = m_tiles.at(index.row());

    switch (role) {
    case TileRefRole: {
        QVariantMap id;
        id["tileId"] = QString::fromStdString(tile.tileId);
        id["topLatitude"] = tile.boundingBox.top();
        id["leftLongitude"] = tile.boundingBox.left();
        id["bottomLatitude"] = tile.boundingBox.bottom();
        id["rightLongitude"] = tile.boundingBox.right();
        id["maxPixelsPerLongitude"] = tile.maxPixelsPerLon;
        return id;
    } break;
    case TileIdRole:
        return QString::fromStdString(tile.tileId);
    case TopLatitude:
        return tile.boundingBox.top();
    case LeftLongitude:
        return tile.boundingBox.left();
    case BottomLatitude:
        return tile.boundingBox.bottom();
    case RightLongitude:
        return tile.boundingBox.right();
    }

    return QVariant();
}

QHash<int, QByteArray> MapTileModel::roleNames() const
{
    return m_roleNames;
}

QSizeF MapTileModel::viewPort() const
{
    return m_viewPort;
}

QVariantMap MapTileModel::tileRefAtPos(float lat, float lon)
{
    for (const TileFactory::Tile &tile : m_tiles) {
        if (tile.boundingBox.contains(lat, lon)) {
            return createTileRef(QString::fromStdString(tile.tileId),
                                 tile.boundingBox,
                                 tile.maxPixelsPerLon);
        }
    }
    return {};
}
