#pragma once

#include <QObject>
#include <QRectF>
#include <QSGTexture>
#include <QtConcurrent>

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
        QList<float> symbolVertices;
        QList<float> textVertices;
    };

    Tessellator(TileFactoryWrapper *tileFactory,
                TileFactoryWrapper::TileRecipe,
                const SymbolImage *symbolImage,
                const FontImage *fontImage);

    void fetchAgain();
    QList<float> textVertices() const { return m_data.textVertices; }
    QList<float> symbolVertices() const { return m_data.symbolVertices; }
    bool isReady() const { return m_ready; }
    bool hasDataChanged() const { return m_dataChanged; }
    bool isRemoved() const { return m_removed; }
    void setRemoved();
    void resetDataChanged() { m_dataChanged = false; }
    void setTileFactory(TileFactoryWrapper *tileFactoryWrapper);
    TileData data() const { return m_data; }
    QString id() const { return m_tileId; }
    static int pixelsPerLon();

public slots:
    void finished();

signals:
    void dataChanged();

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

    static QList<float> addSymbols(const QList<Symbol> &input);
    static QList<float> addLabels(const QList<SymbolLabel> &labels,
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
                              const SymbolImage *symbolImage,
                              const FontImage *fontImage);

    QFuture<TileData> m_result;
    QFutureWatcher<TileData> m_watcher;
    QString m_tileId;
    TileData m_data;
    TileFactoryWrapper::TileRecipe m_recipe;
    TileFactoryWrapper *m_tileFactory = nullptr;
    const SymbolImage *m_symbolImage = nullptr;
    const FontImage *m_fontImage = nullptr;
    bool m_removed = false;
    bool m_ready = false;
    bool m_dataChanged = true;
    bool m_fetchAgain = false;
};
