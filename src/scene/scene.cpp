#include <QtConcurrent>

#include "rootnode.h"
#include "scene/scene.h"
#include "tessellator.h"
#include "tilefactory/mercator.h"
#include "tilenode.h"

#ifndef SYMBOLS_DIR
#error SYMBOLS_DIR must be defined
#endif

Scene::Scene(QQuickItem *parent)
    : m_symbolImage(SYMBOLS_DIR)
{
    setFlag(ItemHasContents, true);
}

void Scene::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    m_boundingRect = newGeometry;
    updateBox();
    update();
}

void Scene::updateBox()
{
    QPointF topLeft;
    {
        double x = Mercator::mercatorWidth(0, m_viewport.x(), Tessellator::pixelsPerLon());
        double y = Mercator::mercatorHeight(0, m_viewport.y(), Tessellator::pixelsPerLon());
        topLeft = QPointF(x, y);
    }

    QRectF rect = boundingRect();

    double lon = Mercator::mercatorWidthInverse(m_viewport.x(), rect.width(), m_viewport.z());
    double lat = Mercator::mercatorHeightInverse(m_viewport.y(), rect.height(), m_viewport.z());
    QPointF gpsBottom(lon, lat);

    QPointF bottomRight;
    {
        double x = Mercator::mercatorWidth(0, gpsBottom.x(), Tessellator::pixelsPerLon());
        double y = Mercator::mercatorHeight(0, gpsBottom.y(), Tessellator::pixelsPerLon());
        bottomRight = QPointF(x, y);
    }

    m_box = QRectF(topLeft, bottomRight);
}

QSGNode *Scene::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    auto *rootNode = static_cast<RootNode *>(old);

    QSGTransformNode *transformNode = nullptr;

    if (!rootNode) {
        rootNode = new RootNode(m_symbolImage, m_fontImage, window());
        transformNode = new QSGTransformNode();
        rootNode->appendChildNode(transformNode);
    }

    QRectF box = boundingRect();

    QMatrix4x4 matrix;

    transformNode = static_cast<QSGTransformNode *>(rootNode->firstChild());

    float zoom = box.width() / m_box.width();
    if (!m_box.isNull()) {
        float xScale = box.width() / m_box.width();
        float yScale = box.height() / m_box.height();
        matrix.scale(xScale, yScale);
        matrix.translate(-m_box.x(), -m_box.y());
        transformNode->setMatrix(matrix);
    }

    if (m_tilesChanged) {
        auto *tileNode = static_cast<TileNode *>(transformNode->firstChild());

        QStringList tileIds;

        while (tileNode) {
            auto *next = static_cast<TileNode *>(tileNode->nextSibling());
            if (m_tiles.contains(tileNode->tileId())) {
                tileIds.append(tileNode->tileId());
            } else {
                transformNode->removeChildNode(tileNode);
                delete tileNode;
            }
            tileNode = next;
        }

        for (const auto &tileId : m_tiles.keys()) {
            if (!tileIds.contains(tileId)) {
                auto *tileNode = new TileNode(tileId,
                                              m_tileFactory,
                                              &m_fontImage,
                                              &m_symbolImage,
                                              { rootNode->fontTexture(),
                                                rootNode->symbolTexture() },
                                              m_tiles[tileId]);
                connect(tileNode, &TileNode::update, this, [&]() {
                    update();
                });
                transformNode->appendChildNode(tileNode);
            }
        }
    }

    auto *tileNode = static_cast<TileNode *>(transformNode->firstChild());
    while (tileNode) {
        tileNode->updatePaintNode(zoom);

        if (m_sourceDataChanged) {
            tileNode->setTileFactory(m_tileFactory);
            tileNode->fetchAgain();
        }

        tileNode = static_cast<TileNode *>(tileNode->nextSibling());
    }

    m_sourceDataChanged = false;
    return rootNode;
}

TileFactoryWrapper *Scene::tileFactory() const
{
    return m_tileFactory;
}

void Scene::setTileFactory(TileFactoryWrapper *newTileFactory)
{
    if (m_tileFactory == newTileFactory)
        return;
    m_tileFactory = newTileFactory;
    emit tileFactoryChanged();
    m_sourceDataChanged = true;
}

const QVector3D &Scene::viewport() const
{
    return m_viewport;
}

void Scene::setViewport(const QVector3D &newViewport)
{
    if (m_viewport == newViewport)
        return;
    m_viewport = newViewport;
    emit viewportChanged();

    updateBox();
    update();
}

QAbstractListModel *Scene::tileModel() const
{
    return m_tileModel;
}

void Scene::setModel(QAbstractListModel *newTileModel)
{
    if (m_tileModel == newTileModel)
        return;

    if (m_tileModel) {
        disconnect(m_tileModel, nullptr, this, nullptr);
    }

    m_tileModel = newTileModel;
    emit tileModelChanged();

    if (!m_tileModel) {
        return;
    }

    connect(m_tileModel, &QAbstractListModel::rowsInserted,
            this, &Scene::rowsInserted);

    connect(m_tileModel, &QAbstractListModel::rowsAboutToBeRemoved,
            this, &Scene::rowsAboutToBeRemoved);

    connect(m_tileModel, &QAbstractListModel::dataChanged,
            this, &Scene::dataChanged);

    if (m_tileModel->rowCount() > 0) {
        addTilesFromModel(m_tileFactory, 0, m_tileModel->rowCount() - 1);
    }
}

void Scene::addTilesFromModel(TileFactoryWrapper *tileFactory, int first, int last)
{
    for (int i = first; i < last + 1; i++) {
        QVariant tileRefVariant = m_tileModel->data(m_tileModel->index(i, 0), 0);
        QVariantMap tileRef = tileRefVariant.toMap();
        QString tileId = tileRef[QStringLiteral("tileId")].toString();
        auto result = parseTileRef(tileRef);
        if (result.has_value()) {
            m_tiles.insert(tileId, result.value());
            m_tilesChanged = true;
        } else {
            qWarning() << "Failed to parse tile ref";
        }
    }
}

void Scene::rowsInserted(const QModelIndex &parent, int first, int last)
{
    addTilesFromModel(m_tileFactory, first, last);
}

void Scene::dataChanged(const QModelIndex &topLeft,
                        const QModelIndex &bottomRight,
                        const QList<int> &roles)
{

    m_sourceDataChanged = true;
    update();
}

void Scene::rowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    QStringList ids;

    for (int i = first; i < last + 1; i++) {
        QVariant tileRefVariant = m_tileModel->data(m_tileModel->index(i, 0), 0);
        QVariantMap tileRef = tileRefVariant.toMap();
        QString tileId = tileRef["tileId"].toString();

        if (m_tiles.contains(tileId)) {
            m_tiles.remove(tileId);
        } else {
            qWarning() << "Strange missing id" << tileId;
        }
    }

    m_tilesChanged = true;
}

std::optional<TileFactoryWrapper::TileRecipe> Scene::parseTileRef(const QVariantMap &tileRef)
{
    bool ok;
    const double topLatitude = tileRef["topLatitude"].toDouble(&ok);
    if (!ok) {
        qWarning() << "Failed to convert topLatitude";
        return {};
    }
    const double leftLongitude = tileRef["leftLongitude"].toDouble(&ok);
    if (!ok) {
        qWarning() << "Failed to convert leftLongitude";
        return {};
    }
    const double bottomLatitude = tileRef["bottomLatitude"].toDouble(&ok);
    if (!ok) {
        qWarning() << "Failed to convert bottomLatitude";
        return {};
    }
    const double rightLongitude = tileRef["rightLongitude"].toDouble(&ok);
    if (!ok) {
        qWarning() << "Failed to convert rightLongitude";
        return {};
    }
    const int maxPixelsPerLongitude = tileRef["maxPixelsPerLongitude"].toInt(&ok);
    if (!ok) {
        qWarning() << "Failed to convert maxPixelsPerLongitude";
        return {};
    }

    TileFactoryWrapper::TileRecipe recipe = {
        GeoRect(topLatitude, bottomLatitude, leftLongitude, rightLongitude),
        static_cast<double>(maxPixelsPerLongitude)
    };

    return recipe;
}
