#pragma once

#include <memory>

#include <QObject>
#include <QRectF>
#include <QSGTexture>
#include <QtConcurrent>

#include "scene/annotation/annotationnode.h"
#include "scene/fontimage.h"
#include "scene/symbolimage.h"
#include "tilefactory/chart.h"
#include "tilefactorywrapper.h"

/*!
    Processes chart data for a tile an creates vertices for scene graph
*/
class Tessellator : public QObject
{
    Q_OBJECT
public:
    struct TileData
    {
        QList<AnnotationNode::Vertex> symbolVertices;
        QList<AnnotationNode::Vertex> textVertices;
    };

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
    TileData data() const { return m_data; }
    QString id() const { return m_id; }
    void setId(const QString &tileId);
    static int pixelsPerLon();

public slots:
    void finished();

signals:
    void dataChanged(const QString &id);

private:
    // This only applies to symbol collisions. Labels are always checked.
    enum class CollisionRule {
        NoCheck,
        Always,
        OnlyWithSameType
    };

    struct SymbolLabel
    {
        QPointF pos;
        QString text;
        QPointF offset;
        QRectF boundingBox;
        FontImage::FontType font;
        QColor color;
        float pointSize;
        std::optional<float> scaleLimit;
        std::optional<float> parentScaleLimit;
    };

    struct Symbol
    {
        QPointF pos;
        std::optional<SymbolImage::TextureSymbol> symbol;
        std::optional<float> scaleLimit;
        QList<SymbolLabel> labels;
        int priority = 0;
        CollisionRule collisionRule;
    };

    static QPointF posToMeractor(const ChartData::Position::Reader &pos);
    static QRectF computeSymbolBox(const QTransform &transform,
                                   const QPointF &pos,
                                   SymbolImage::TextureSymbol &textureSymbol);

    static QList<AnnotationNode::Vertex> addSymbols(const QList<Symbol> &input);
    static QList<AnnotationNode::Vertex> addLabels(const QList<SymbolLabel> &labels,
                                                   const FontImage *fontImage);

    enum class LabelPlacement {
        Below,
    };

    static QPointF labelOffset(const QRectF &labelBox,
                               const SymbolImage::TextureSymbol &symbol,
                               LabelPlacement placement);

    /*!
        Fetch data from the tilefactory and converts to vertex data
    */
    static TileData fetchData(TileFactoryWrapper *tileFactory,
                              TileFactoryWrapper::TileRecipe recipe,
                              std::shared_ptr<const SymbolImage> symbolImage,
                              std::shared_ptr<const FontImage> fontImage);

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
