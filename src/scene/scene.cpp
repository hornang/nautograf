#include <QtConcurrent>

#include "annotation/annotationmaterial.h"
#include "annotation/annotationnode.h"
#include "fontimage.h"
#include "geometrynode.h"
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
        if (!m_tessellators.contains(tile->id())) {
            parent->removeChildNode(tile);
            delete tile;
        }
        tile = next;
    }
}

void Scene::markChildrensDirtyMaterial(QSGNode *parent)
{
    QSGNode *tile = static_cast<QSGNode *>(parent->firstChild());

    while (tile) {
        tile->markDirty(QSGNode::DirtyMaterial);
        tile = static_cast<QSGNode *>(tile->nextSibling());
    }
}

namespace {

struct Materials
{
    PolygonMaterial *polygon = nullptr;
    AnnotationMaterial *symbol = nullptr;
    AnnotationMaterial *text = nullptr;
};

void deleteChildNodes(QSGNode *parent)
{
    QSGNode *child = parent->firstChild();
    while (child) {
        QSGNode *nextChild = child->nextSibling();
        delete child;
        child = nextChild;
    }
    parent->removeAllChildNodes();
}

template <typename T>
T *findChild(const QSGNode *parent, const QString &id)
{
    Q_ASSERT(parent);

    T *tile = static_cast<T *>(parent->firstChild());
    while (tile) {
        if (id == tile->id()) {
            return tile;
        }
        tile = static_cast<T *>(tile->nextSibling());
    }

    return nullptr;
}

void updateGeometryLayers(const QString &tileId,
                          QSGNode *parent,
                          const QList<GeometryLayer> &newData,
                          const Materials &materials)
{
    GeometryNode *geometryNode = findChild<GeometryNode>(parent, tileId);

    if (geometryNode) {
        deleteChildNodes(geometryNode);
    } else {
        geometryNode = new GeometryNode(tileId);
        geometryNode->setFlag(QSGNode::OwnedByParent, true);
        parent->appendChildNode(geometryNode);
    }

    assert(geometryNode);

    for (const GeometryLayer &layer : newData) {
        QSGNode *layerNode = new QSGNode();
        layerNode->setFlag(QSGNode::OwnedByParent, true);
        geometryNode->appendChildNode(layerNode);
        layerNode->appendChildNode(new PolygonNode(tileId,
                                                   materials.polygon,
                                                   layer.polygonVertices));
    }
}

void updateAnnotationNode(const QString &tileId,
                          QSGNode *parent,
                          std::function<QList<AnnotationNode::Vertex>(const TileData &tileData)> getter,
                          QSGMaterial *material,
                          const QHash<QString, std::shared_ptr<Tessellator>> &tessellators)
{
    auto *node = findChild<AnnotationNode>(parent, tileId);
    const std::shared_ptr<Tessellator> &tessellator = tessellators[tileId];

    if (node) {
        node->updateVertices(getter(tessellator->data()));
    } else {
        parent->appendChildNode(new AnnotationNode(tileId, material, getter(tessellator->data())));
    }
}
}

QSGNode *Scene::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    RootNode *rootNode = static_cast<RootNode *>(old);

    QSGNode *geometryNodesParent = nullptr;
    QSGNode *symbolNodesParent = nullptr;
    QSGNode *textNodesParent = nullptr;
    QSGNode *overlayNodesParent = nullptr;

    if (!rootNode) {
        rootNode = new RootNode(m_symbolImage->image(),
                                m_fontImage->image(),
                                window());

        geometryNodesParent = new QSGNode();
        rootNode->appendChildNode(geometryNodesParent);
        symbolNodesParent = new QSGNode();
        rootNode->appendChildNode(symbolNodesParent);
        textNodesParent = new QSGNode();
        rootNode->appendChildNode(textNodesParent);
        overlayNodesParent = new QSGNode();
        rootNode->appendChildNode(overlayNodesParent);

    } else {
        geometryNodesParent = rootNode->firstChild();
        Q_ASSERT(geometryNodesParent);
        symbolNodesParent = geometryNodesParent->nextSibling();
        Q_ASSERT(symbolNodesParent);
        textNodesParent = symbolNodesParent->nextSibling();
        Q_ASSERT(textNodesParent);
        overlayNodesParent = textNodesParent->nextSibling();
        Q_ASSERT(overlayNodesParent);
    }

    Materials materials;
    materials.polygon = rootNode->polygonMaterial();
    materials.symbol = rootNode->symbolMaterial();
    materials.text = rootNode->textMaterial();

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
        removeStaleNodes<GeometryNode>(geometryNodesParent);
        removeStaleNodes<AnnotationNode>(symbolNodesParent);
        removeStaleNodes<AnnotationNode>(textNodesParent);
    }

    if (!m_tessellatorsWithPendingData.empty()) {
        for (const auto &tileId : m_tessellatorsWithPendingData) {
            Q_ASSERT(m_tessellators.contains(tileId));

            updateGeometryLayers(tileId,
                                 geometryNodesParent,
                                 m_tessellators[tileId]->data().geometryLayers,
                                 materials);

            updateAnnotationNode(
                tileId, symbolNodesParent,
                [&](const TileData &tileData) {
                    return tileData.symbolVertices;
                },
                materials.symbol,
                m_tessellators);

            updateAnnotationNode(
                tileId, textNodesParent,
                [&](const TileData &tileData) {
                    return tileData.textVertices;
                },
                materials.text,
                m_tessellators);
        }
        m_tessellatorsWithPendingData.clear();
    }

    if (m_focusedTileChanged || m_tessellatorRemoved || m_newTessellators.contains(m_focusedTile)) {
        if (m_tessellators.contains(m_focusedTile)) {
            PolygonNode *polygonNode = nullptr;
            if (overlayNodesParent->childCount() > 0) {
                Q_ASSERT(overlayNodesParent->childCount() == 1);
                polygonNode = static_cast<PolygonNode *>(overlayNodesParent->firstChild());
                polygonNode->updateVertices(m_tessellators[m_focusedTile]->overlayVertices(m_accentColor));
            } else {
                PolygonNode *polygonNode = new PolygonNode(m_focusedTile,
                                                           rootNode->blendColorMaterial(),
                                                           m_tessellators[m_focusedTile]->overlayVertices(m_accentColor));
                overlayNodesParent->appendChildNode(polygonNode);
            }
        } else {
            if (overlayNodesParent->childCount() > 0) {
                Q_ASSERT(overlayNodesParent->childCount() == 1);
                QSGNode *child = overlayNodesParent->firstChild();
                overlayNodesParent->removeChildNode(child);
                delete child;
            }
        }
    }

    m_focusedTileChanged = false;
    m_tessellatorRemoved = false;
    m_newTessellators.clear();

    if (m_zoom != zoom) {
        materials.symbol->setScale(zoom);
        markChildrensDirtyMaterial(symbolNodesParent);
        materials.text->setScale(zoom);
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
    connect(m_tileFactory, &TileFactoryWrapper::tileDataChanged, this, &Scene::tileDataChanged);
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
            m_newTessellators.insert(tileId);
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

void Scene::tileDataChanged(const QStringList &tileIds)
{
    for (const QString &tileId : tileIds) {
        if (m_tessellators.contains(tileId)) {
            m_tessellators[tileId]->fetchAgain();
        }
    }
}

const QString &Scene::focusedTile() const
{
    return m_focusedTile;
}

void Scene::setFocusedTile(const QString &newFocusedTile)
{
    if (m_focusedTile == newFocusedTile)
        return;
    m_focusedTile = newFocusedTile;
    emit setFocusedTileChanged();
    m_focusedTileChanged = true;
    update();
}

const QColor &Scene::accentColor() const
{
    return m_accentColor;
}

void Scene::setAccentColor(const QColor &newAccentColor)
{
    if (m_accentColor == newAccentColor)
        return;
    m_accentColor = newAccentColor;
    emit accentColorChanged();
}
