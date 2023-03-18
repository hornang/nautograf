#pragma once

#include <memory>

#include <QObject>
#include <QRectF>
#include <QSGTexture>
#include <QtConcurrent>

#include "annotation/annotationnode.h"
#include "fontimage.h"
#include "polygon/polygonnode.h"
#include "scene/tilefactorywrapper.h"
#include "symbolimage.h"
#include "tilefactory/chart.h"

#include "tiledata.h"

/*!
    Processes chart data for a tile an creates vertices for scene graph
*/
class Tessellator : public QObject
{
    Q_OBJECT
public:
    Tessellator(TileFactoryWrapper *tileFactory,
                TileFactoryWrapper::TileRecipe,
                std::shared_ptr<const SymbolImage> symbolImage,
                std::shared_ptr<const FontImage> fontImage);

    void fetchAgain();
    QList<AnnotationNode::Vertex> textVertices() const { return m_data.textVertices; }
    QList<AnnotationNode::Vertex> symbolVertices() const { return m_data.symbolVertices; }
    bool isReady() const { return m_ready; }
    bool hasDataChanged() const { return m_dataChanged; }
    bool isRemoved() const { return m_removed; }
    void setRemoved();
    void resetDataChanged() { m_dataChanged = false; }
    void setTileFactory(TileFactoryWrapper *tileFactoryWrapper);
    const TileData &data() const { return m_data; }
    QString id() const { return m_id; }
    void setId(const QString &tileId);
    static int pixelsPerLon();
    QList<PolygonNode::Vertex> overlayVertices(const QColor &color) const;

public slots:
    void finished();

signals:
    void dataChanged(const QString &id);

private:
    QFuture<TileData> m_result;
    QFutureWatcher<TileData> m_watcher;
    QString m_id;
    TileData m_data;
    TileFactoryWrapper::TileRecipe m_recipe;
    TileFactoryWrapper *m_tileFactory = nullptr;
    std::shared_ptr<const SymbolImage> m_symbolImage;
    std::shared_ptr<const FontImage> m_fontImage;
    bool m_removed = false;
    bool m_ready = false;
    bool m_dataChanged = true;
    bool m_fetchAgain = false;
};
