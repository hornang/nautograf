#include <QtConcurrent>

#include "annotation/annotationmaterial.h"
#include "annotation/annotationnode.h"
#include "fontimage.h"
#include "polygon/polygonmaterial.h"
#include "polygon/polygonnode.h"
#include "rootnode.h"
#include "scene/scene.h"
#include "symbolimage.h"
#include "tessellator.h"
#include "tilefactory/mercator.h"

#ifndef SYMBOLS_DIR
#error SYMBOLS_DIR must be defined
#endif

Scene::Scene(QQuickItem *parent)
    : m_symbolImage(std::make_shared<SymbolImage>(SYMBOLS_DIR))
    , m_fontImage(std::make_shared<FontImage>())
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
        double x = Mercator::mercatorWidth(0, m_lon, Tessellator::pixelsPerLon());
        double y = Mercator::mercatorHeight(0, m_lat, Tessellator::pixelsPerLon());
        topLeft = QPointF(x, y);
    }

    QRectF rect = boundingRect();

    double lon = Mercator::mercatorWidthInverse(m_lon, rect.width(), m_pixelsPerLon);
    double lat = Mercator::mercatorHeightInverse(m_lat, rect.height(), m_pixelsPerLon);
    QPointF gpsBottom(lon, lat);

    QPointF bottomRight;
    {
        double x = Mercator::mercatorWidth(0, gpsBottom.x(), Tessellator::pixelsPerLon());
        double y = Mercator::mercatorHeight(0, gpsBottom.y(), Tessellator::pixelsPerLon());
        bottomRight = QPointF(x, y);
    }

    m_box = QRectF(topLeft, bottomRight);
}

template <typename T>
void Scene::removeStaleNodes(QSGNode *parent) const
{
    Q_ASSERT(parent);

    T *tile = static_cast<T *>(parent->firstChild());
    while (tile) {
        auto *next = static_cast<T *>(tile->nextSibling());
        if (!m_tessellators.contains(tile->tileId())) {
            parent->removeChildNode(tile);
            delete tile;
        }
        tile = next;
    }
}

template <typename T>
QHash<QString, T *> Scene::currentNodes(const QSGNode *parent)
{
    Q_ASSERT(parent);

    QHash<QString, T *> nodes;

    T *tile = static_cast<T *>(parent->firstChild());
    while (tile) {
        nodes[tile->tileId()] = tile;
        auto *next = static_cast<T *>(tile->nextSibling());
        tile = next;
    }

    return nodes;
}

void Scene::markChildrensDirtyMaterial(QSGNode *parent)
{
    QSGNode *tile = static_cast<QSGNode *>(parent->firstChild());

    while (tile) {
        tile->markDirty(QSGNode::DirtyMaterial);
        tile = static_cast<QSGNode *>(tile->nextSibling());
    }
}

template <typename T>
void Scene::updateNodeData(const QString &tileId,
                           QSGNode *parent,
                           const QHash<QString, T *> &existingNodes,
                           std::function<QList<typename T::Vertex>(const Tessellator::TileData &tileData)> getter,
                           QSGMaterial *material)
{
    const std::shared_ptr<Tessellator> &tessellator = m_tessellators[tileId];

    if (existingNodes.contains(tileId)) {
        existingNodes[tileId]->updateVertices(getter(tessellator->data()));
    } else {
        parent->appendChildNode(new T(tileId, material, getter(tessellator->data())));
    }
}

QSGNode *Scene::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    RootNode *rootNode = static_cast<RootNode *>(old);

    QSGNode *polygonNodesParent = nullptr;
    QSGNode *symbolNodesParent = nullptr;
    QSGNode *textNodesParent = nullptr;

    if (!rootNode) {
        rootNode = new RootNode(m_symbolImage->image(),
                                m_fontImage->image(),
                                window());

        polygonNodesParent = new QSGNode();
        rootNode->appendChildNode(polygonNodesParent);
        symbolNodesParent = new QSGNode();
        rootNode->appendChildNode(symbolNodesParent);
        textNodesParent = new QSGNode();
        rootNode->appendChildNode(textNodesParent);
    } else {
        polygonNodesParent = rootNode->firstChild();
        symbolNodesParent = polygonNodesParent->nextSibling();
        textNodesParent = symbolNodesParent->nextSibling();
    }

    Q_ASSERT(polygonNodesParent);
    Q_ASSERT(symbolNodesParent);
    Q_ASSERT(textNodesParent);

    PolygonMaterial *polygonMaterial = rootNode->polygonMaterial();
    AnnotationMaterial *symbolMaterial = rootNode->symbolMaterial();
    AnnotationMaterial *textMaterial = rootNode->textMaterial();

    QRectF box = boundingRect();

    QMatrix4x4 matrix;

    float zoom = box.width() / m_box.width();
    if (!m_box.isNull()) {
        float xScale = box.width() / m_box.width();
        float yScale = box.height() / m_box.height();
        matrix.scale(xScale, yScale);
        matrix.translate(-m_box.x(), -m_box.y());
        rootNode->setMatrix(matrix);
    }

    if (m_tessellatorRemoved) {
        removeStaleNodes<PolygonNode>(polygonNodesParent);
        removeStaleNodes<AnnotationNode>(symbolNodesParent);
        removeStaleNodes<AnnotationNode>(textNodesParent);
        m_tessellatorRemoved = false;
    }

    if (!m_tessellatorsWithPendingData.empty()) {
        QHash<QString, PolygonNode *> polygonNodes = currentNodes<PolygonNode>(polygonNodesParent);
        QHash<QString, AnnotationNode *> symbolNodes = currentNodes<AnnotationNode>(symbolNodesParent);
        QHash<QString, AnnotationNode *> textNodes = currentNodes<AnnotationNode>(textNodesParent);

        for (const auto &tileId : m_tessellatorsWithPendingData) {
            Q_ASSERT(m_tessellators.contains(tileId));

            updateNodeData<PolygonNode>(
                tileId,
                polygonNodesParent,
                polygonNodes,
                [&](const Tessellator::TileData &tileData) {
                    return tileData.polygonVertices;
                },
                polygonMaterial);

            updateNodeData<AnnotationNode>(
                tileId,
                symbolNodesParent,
                symbolNodes,
                [&](const Tessellator::TileData &tileData) {
                    return tileData.symbolVertices;
                },
                symbolMaterial);

            updateNodeData<AnnotationNode>(
                tileId,
                textNodesParent,
                textNodes,
                [&](const Tessellator::TileData &tileData) {
                    return tileData.textVertices;
                },
                textMaterial);
        }
        m_tessellatorsWithPendingData.clear();
    }

    if (m_zoom != zoom) {
        symbolMaterial->setScale(zoom);
        markChildrensDirtyMaterial(symbolNodesParent);
        textMaterial->setScale(zoom);
        markChildrensDirtyMaterial(textNodesParent);
    }
    m_zoom = zoom;

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

    if (m_tileFactory) {
        disconnect(m_tileFactory, nullptr, this, nullptr);
    }

    m_tileFactory = newTileFactory;
    emit tileFactoryChanged();

    fetchAll();
}

double Scene::lat() const
{
    return m_lat;
}

void Scene::setLat(double newLat)
{
    if (qFuzzyCompare(m_lat, newLat))
        return;
    m_lat = newLat;
    emit latChanged();

    updateBox();
    update();
}

double Scene::lon() const
{
    return m_lon;
}

void Scene::setLon(double newLon)
{
    if (qFuzzyCompare(m_lon, newLon))
        return;
    m_lon = newLon;
    emit lonChanged();

    updateBox();
    update();
}

double Scene::pixelsPerLon() const
{
    return m_pixelsPerLon;
}

void Scene::setPixelsPerLon(double newPixelsPerLon)
{
    if (qFuzzyCompare(m_pixelsPerLon, newPixelsPerLon))
        return;
    m_pixelsPerLon = newPixelsPerLon;
    emit pixelsPerLonChanged();

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
        addTessellatorsFromModel(m_tileFactory, 0, m_tileModel->rowCount() - 1);
    }
}

void Scene::addTessellatorsFromModel(TileFactoryWrapper *tileFactory, int first, int last)
{
    for (int i = first; i < last + 1; i++) {
        QVariant tileRefVariant = m_tileModel->data(m_tileModel->index(i, 0), 0);
        QVariantMap tileRef = tileRefVariant.toMap();
        QString tileId = tileRef[QStringLiteral("tileId")].toString();
        auto result = parseTileRef(tileRef);
        if (result.has_value()) {
            auto tessellator = std::make_shared<Tessellator>(m_tileFactory,
                                                             result.value(),
                                                             m_symbolImage,
                                                             m_fontImage);
            connect(tessellator.get(), &Tessellator::dataChanged, this, &Scene::tessellatorDone);
            tessellator->setId(tileId);
            m_tessellators.insert(tileId, tessellator);
            tessellator->fetchAgain();
        } else {
            qWarning() << "Failed to parse tile ref";
        }
    }
}

void Scene::rowsInserted(const QModelIndex &parent, int first, int last)
{
    addTessellatorsFromModel(m_tileFactory, first, last);
}

void Scene::fetchAll()
{
    QHashIterator<QString, std::shared_ptr<Tessellator>> i(m_tessellators);

    while (i.hasNext()) {
        i.next();
        i.value()->fetchAgain();
    }
}

void Scene::dataChanged(const QModelIndex &topLeft,
                        const QModelIndex &bottomRight,
                        const QList<int> &roles)
{
    fetchAll();
}

void Scene::rowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    QStringList ids;

    for (int i = first; i < last + 1; i++) {
        QVariant tileRefVariant = m_tileModel->data(m_tileModel->index(i, 0), 0);
        QVariantMap tileRef = tileRefVariant.toMap();
        QString tileId = tileRef["tileId"].toString();

        if (m_tessellators.contains(tileId)) {
            m_tessellators.remove(tileId);
            m_tessellatorsWithPendingData.remove(tileId);
            m_tessellatorRemoved = true;
        } else {
            qWarning() << "Strange missing id" << tileId;
        }
    }
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

void Scene::tessellatorDone(const QString &tileId)
{
    if (m_tessellatorsWithPendingData.contains(tileId)) {
        return;
    }
    m_tessellatorsWithPendingData.insert(tileId);
    update();
}
