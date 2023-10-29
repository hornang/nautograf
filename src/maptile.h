#pragma once

#include "tilefactory/chart.h"
#include "tilefactory/georect.h"

#include <QVector3D>
#include <QtQuick>
#include <QtConcurrent>
#include <QFont>

class TileFactoryWrapper;

class MapTile : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap tileRef READ tileRef WRITE setTileRef NOTIFY tileRefChanged)
    Q_PROPERTY(TileFactoryWrapper *tileFactory READ tileFactory WRITE setTileFactory NOTIFY tileFactoryChanged)
    Q_PROPERTY(double lat READ lat WRITE setLat NOTIFY latChanged)
    Q_PROPERTY(double lon READ lon WRITE setLon NOTIFY lonChanged)
    Q_PROPERTY(double pixelsPerLon READ pixelsPerLon WRITE setPixelsPerLon NOTIFY pixelsPerLonChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(int margin READ margin CONSTANT)
    Q_PROPERTY(bool noData READ noData NOTIFY noDataChanged)
    Q_PROPERTY(bool error READ error NOTIFY errorChanged)
    Q_PROPERTY(QStringList charts READ charts NOTIFY chartsChanged)

public:
    MapTile(QQuickItem *parent = 0);
    ~MapTile();

    double lat() const;
    void setLat(double newLat);

    double lon() const;
    void setLon(double newLon);

    double pixelsPerLon() const;
    void setPixelsPerLon(double newPixelsPerLon);

    void paint(QPainter *painter);
    const QVariantMap &tileRef() const;
    void setTileRef(const QVariantMap &newTileId);
    TileFactoryWrapper *tileFactory() const;
    void setTileFactory(TileFactoryWrapper *newTileFactory);
    bool loading() const;
    int margin() const;
    QStringList charts() const;
    bool chartVisible(int index) const;
    bool noData() const;
    bool error() const;

public slots:
    void setChartVisibility(const QString &name, bool visible);
    bool chartVisible(const QString &name) const;
    void renderFinished();

private:
    struct RenderConfig {
        /// Geodetic position of paint target's origin
        Pos topLeft;

        /// Ofset everything with pixels
        QPointF offset;

        qreal pixelsPerLongitude;
        QSizeF size;
        int scale = 0;
        QStringList hiddenCharts;
    };

    void render(double lat, double lon, double pixelsPerLon);
    static std::vector<std::shared_ptr<Chart>> getChartData(TileFactoryWrapper *tileFactory,
                                                                const GeoRect &boundingBox,
                                                                int pixelsPerLongitude);
    /*!
        Renders the tile onto a QImage

        The tile is provided as a shared_ptr to ensure the life-time of the tile.
    */
    static QPair<QImage, std::vector<std::shared_ptr<Chart>>> renderTile(TileFactoryWrapper *tileFactory,
                                                                             RenderConfig renderConfig,
                                                                             const GeoRect &boundingBox,
                                                                             int maxPixelsPerLongitude);
    static void paintLandArea(const ::capnp::List<ChartData::LandArea>::Reader &areas,
                              const RenderConfig &renderConfig,
                              QPainter *painter);

    static void paintDepthAreas(const ::capnp::List<ChartData::DepthArea>::Reader &depthAreas,
                                const RenderConfig &renderConfig,
                                QPainter *painter);

    static void paintBuiltUpArea(const ::capnp::List<ChartData::BuiltUpArea>::Reader &areas,
                                 const RenderConfig &renderConfig,
                                 QPainter *painter);

    static void clipAroundCoverage(const capnp::List<ChartData::CoverageArea>::Reader &areas,
                                   const RenderConfig &renderConfig,
                                   QPainter *painter);

    static inline QPointF toMercator(const Pos &topLeft,
                                     qreal pixelsPerLongitude,
                                     const Pos &position);

    static inline QPointF toMercator(const Pos &topLeft,
                                     qreal pixelsPerLongitude,
                                     const double &lat,
                                     const double &lon);

    static void paintRoads(const capnp::List<ChartData::Road>::Reader &roads,
                           const RenderConfig &renderConfig,
                           QPainter *painter);

    static QPolygonF fromCapnpToPolygon(const capnp::List<ChartData::Position>::Reader src,
                                        const RenderConfig &renderConfig);

    static QPolygonF reversePolygon(const QPolygonF &input);
    static QSizeF sizeFromPixelsPerLon(const GeoRect &boundingBox, double pixelsPerLon);
    static void rasterClip(const QRectF &outer, const QRectF &inner, QPainter *painter);
    static QColor depthColor(qreal depth);

    void updateGeometry();

    QImage m_image;
    GeoRect m_boundingBox;
    QString m_tileId;
    QFuture<QPair<QImage, std::vector<std::shared_ptr<Chart>>>> m_renderResult;
    QFutureWatcher<QPair<QImage, std::vector<std::shared_ptr<Chart>>>> m_renderResultWatcher;
    QSize m_imageSize;
    QVariantMap m_tileRef;

    double m_lat = 0;
    double m_lon = 0;
    double m_pixelsPerLon = 300;

    double m_updatedLat = 0;
    double m_updatedLon = 0;
    double m_updatedPixelsPerLon = 0;

    TileFactoryWrapper *m_tileFactory = nullptr;
    QStringList m_charts;

    QStringList m_hiddenCharts;

    QList<bool> m_chartVisibility;
    std::vector<std::shared_ptr<Chart>> m_chartDatas;
    bool m_loading = false;
    bool m_pendingRender = false;
    int m_maxPixelsPerLongitude = 3000;
    bool m_noData = false;
    bool m_error = false;

signals:
    void chartsChanged();
    void latChanged();
    void lonChanged();
    void pixelsPerLonChanged();
    void tileRefChanged();
    void tileFactoryChanged(TileFactoryWrapper *tileFactory);
    void loadingChanged(bool loading);
    void marginChanged();
    void symbolsChanged();
    void noDataChanged(bool noData);
    void errorChanged();
};
