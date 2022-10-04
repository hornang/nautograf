#include <QFontMetrics>
#include <QList>
#include <QThread>
#include <QVector>
#include <QtConcurrent>
#include <algorithm>

#include "maptile.h"
#include "tilefactorywrapper.h"
#include "tilefactory/mercator.h"

static const QColor builtUpAreaColor(228, 228, 177);
static const QColor soundingColor(125, 137, 140);

static const char *defaultFont = "fixed";
static const char *beaconFontFamily = "Times New Roman";
static const char *soundingFontFamily = defaultFont;
static const char *underwaterRockFontFamily = defaultFont;
static const char *lateralBuoysFontFamily = defaultFont;

// The QImage render target will be this much larger than the bounding rect
static const QSize extraPixels = QSize(2, 2);

// The MapTile QML item will be this much larger (both width and height) around
// the bounding rectangle.
static constexpr int tilePadding = 40;
static constexpr int beaconLabelMargin = 5;

static const QColor safetyWaterColor(204, 255, 255);
static const QColor shallowWaterColor(153, 204, 255);
static const QColor criticalWaterColor(160, 192, 132);

static const QColor landColor(255, 255, 204);
static const QColor landEdgeColor(181, 181, 181);

MapTile::MapTile(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{

    connect(&m_renderResultWatcher,
            &QFutureWatcher<QImage>::finished,
            this,
            &MapTile::renderFinished);
}

MapTile::~MapTile()
{
    // Only cancels tile creation of tile data that has not yet started. This
    // reduces the number of identical requests in QtConcurrent's queue. Identical
    // requests may still happen dependening on the available slots in the
    // QThreadPool. It is up to the ITileSource to optimize this. For instance
    // avoiding creating the same ChartArea data in two different threads.
    m_renderResult.cancel();
}

QPointF MapTile::offsetPosition(QPointF startPosition,
                                qreal pixelsPerLongitude,
                                QPointF mouseOffset)
{
    double latitude = Mercator::mercatorHeightInverse(startPosition.y(),
                                                      mouseOffset.y(),
                                                      pixelsPerLongitude);
    double longitude = Mercator::mercatorWidthInverse(startPosition.x(),
                                                      mouseOffset.x(),
                                                      pixelsPerLongitude);
    QPointF point(longitude, latitude);
    return point;
}

QPointF MapTile::positionFromViewport(const GeoRect &boundingBox,
                                      const QVector3D &viewport)
{
    if (boundingBox.isNull()) {
        return QPointF();
    }

    double x = Mercator::mercatorWidth(viewport.x(),
                                       boundingBox.left(),
                                       viewport.z());

    double y = Mercator::mercatorHeight(viewport.y(),
                                        boundingBox.top(),
                                        viewport.z());

    return QPointF(x - tilePadding, y - tilePadding);
}

void MapTile::setChartVisibility(const QString &name, bool visible)
{
    if (visible && m_hiddenCharts.contains(name)) {
        m_hiddenCharts.removeAll(name);
    } else if (!visible && !m_hiddenCharts.contains(name)) {
        m_hiddenCharts.append(name);
    } else {
        return;
    }
    render(m_viewport);
}

void MapTile::render(const QVector3D &viewport)
{
    if (m_renderResult.isRunning()) {
        m_pendingRender = true;
        return;
    }

    m_pendingRender = false;
    RenderConfig renderConfig;
    renderConfig.topLeft = m_boundingBox.topLeft();
    renderConfig.size = m_imageSize;
    renderConfig.hiddenCharts = m_hiddenCharts;
    renderConfig.offset = QPointF(extraPixels.width() / 2, extraPixels.height() / 2);
    renderConfig.pixelsPerLongitude = viewport.z();

    if (m_chartDatas.empty()) {
        m_loading = true;
        emit loadingChanged(true);
    }

    m_renderResult = QtConcurrent::run(renderTile,
                                       m_tileFactory,
                                       renderConfig,
                                       m_boundingBox,
                                       m_maxPixelsPerLongitude);
    m_renderResultWatcher.setFuture(m_renderResult);
}

bool MapTile::noData() const
{
    return m_noData;
}

QStringList MapTile::charts() const
{
    return m_charts;
}

void MapTile::renderFinished()
{

    if (m_loading) {
        m_loading = false;
        emit loadingChanged(false);
    }

    if (m_renderResult.resultCount() != 1) {
        qCritical() << "Incorrect result count. Thread probably crashed";
        m_error = true;
        emit errorChanged();
        return;
    }

    m_image = m_renderResult.result().first;
    m_chartDatas = m_renderResult.result().second;
    update();

    m_charts.clear();
    for (const auto &chartData : m_chartDatas) {
        m_charts.push_back(QString::fromStdString(chartData->name()));
    }
    emit chartsChanged();
    m_noData = m_chartDatas.empty();
    emit noDataChanged(m_noData);

    if (m_pendingRender) {
        render(m_viewport);
    }
}

QPair<QImage, std::vector<std::shared_ptr<Chart>>> MapTile::renderTile(TileFactoryWrapper *tileFactory,
                                                                           RenderConfig renderConfig,
                                                                           const GeoRect &boundingBox,
                                                                           int maxPixelsPerLongitude)
{
    if (renderConfig.size.isNull()) {
        return QPair<QImage, std::vector<std::shared_ptr<Chart>>>();
    }

    std::vector<std::shared_ptr<Chart>> chartDatas;
    QSizeF size = renderConfig.size;
    QImage result(size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);

    chartDatas = getChartData(tileFactory,
                              boundingBox,
                              maxPixelsPerLongitude);

    QPainter resultPainter(&result);
    // resultPainter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    result.fill(Qt::transparent);

    QImage image(size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);

    for (const auto &chartData : chartDatas) {
        if (renderConfig.hiddenCharts.contains(QString::fromStdString(chartData->name()))) {
            continue;
        }
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.setRenderHints(QPainter::Antialiasing);

        renderConfig.scale = chartData->nativeScale();
        paintDepthAreas(chartData->depthAreas(), renderConfig, &painter);
        paintLandArea(chartData->landAreas(), renderConfig, &painter);
        paintBuiltUpArea(chartData->builtUpAreas(), renderConfig, &painter);
        paintRoads(chartData->roads(), renderConfig, &painter);

        // Raster clip using the coverage polygon
        clipAroundCoverage(chartData->coverage(), renderConfig, &painter);
        resultPainter.drawImage(QRectF({ 0, 0 }, size), image);
    }

    return QPair<QImage, std::vector<std::shared_ptr<Chart>>>(result, chartDatas);
}

bool MapTile::chartVisible(const QString &name) const
{
    return !m_hiddenCharts.contains(name);
}

QPointF MapTile::toMercator(const Pos &topLeft,
                            qreal pixelsPerLongitude,
                            const Pos &position)
{
    qreal x = Mercator::mercatorWidth(topLeft.lon(), position.lon(), pixelsPerLongitude);
    qreal y = Mercator::mercatorHeight(topLeft.lat(), position.lat(), pixelsPerLongitude);
    return QPointF(x, y);
}

QPointF MapTile::toMercator(const Pos &topLeft,
                            qreal pixelsPerLongitude,
                            const double &lat,
                            const double &lon)
{
    qreal x = Mercator::mercatorWidth(topLeft.lon(), lon, pixelsPerLongitude);
    qreal y = Mercator::mercatorHeight(topLeft.lat(), lat, pixelsPerLongitude);
    return QPointF(x, y);
}

QSizeF MapTile::sizeFromViewport(const GeoRect &boundingBox, const QVector3D &viewport)
{
    if (boundingBox.isNull()) {
        return QSize();
    }

    double width = Mercator::mercatorWidth(boundingBox.left(),
                                           boundingBox.right(),
                                           viewport.z());

    double height = Mercator::mercatorHeight(boundingBox.top(),
                                             boundingBox.bottom(),
                                             viewport.z());
    if (width > 8000) {
        qWarning() << "Width too large";
        width = 8000;
    }

    if (height > 8000) {
        qWarning() << "Height too large";
        height = 8000;
    }

    return QSizeF(width, height);
}

std::vector<std::shared_ptr<Chart>> MapTile::getChartData(TileFactoryWrapper *tileFactory,
                                                              const GeoRect &boundingBox,
                                                              int pixelsPerLongitude)
{
    Q_ASSERT(tileFactory);
    return tileFactory->create({ boundingBox, static_cast<double>(pixelsPerLongitude) });
}

void MapTile::updateGeometry()
{
    if (m_updatedViewport == m_viewport || m_boundingBox.isNull()) {
        return;
    }

    const QPointF position = positionFromViewport(m_boundingBox, m_viewport);

    if (m_updatedViewport.z() != m_viewport.z()) {
        const QSizeF content = sizeFromViewport(m_boundingBox, m_viewport);
        m_imageSize = content.toSize() + extraPixels;
        const QSizeF size = QSize(content.width() + 2. * tilePadding,
                                  content.height() + 2. * tilePadding);

        if (m_updatedViewport.isNull()) {
            setPosition(position);
            setSize(size);
        } else {
            emit newGeometry(position.x(),
                             position.y(),
                             size.width(),
                             size.height());
        }
        render(m_viewport);
    } else {
        setPosition(position);
    }

    m_updatedViewport = m_viewport;
}

void MapTile::setViewport(const QVector3D &viewport)
{
    if (m_viewport == viewport) {
        return;
    }

    m_viewport = viewport;
    emit viewportChanged(viewport);
    updateGeometry();
}

QVector3D MapTile::viewport() const
{
    return m_viewport;
}

void MapTile::paint(QPainter *painter)
{
    RenderConfig renderConfig;
    renderConfig.offset = QPointF(extraPixels.width(), extraPixels.height() / 2);
    renderConfig.topLeft = m_boundingBox.topLeft();
    renderConfig.size = m_imageSize;
    renderConfig.pixelsPerLongitude = m_viewport.z();

    const QRectF targetRect = QRectF(tilePadding - extraPixels.width() / 2,
                                     tilePadding - extraPixels.height() / 2,
                                     width() - 2 * tilePadding + extraPixels.width(),
                                     height() - 2 * tilePadding + extraPixels.height());

    if (!m_image.isNull()) {
        const QRectF sourceRect(QPointF(0, 0), m_image.size());
        painter->drawImage(targetRect, m_image, sourceRect);
    }
}

void MapTile::clipAroundCoverage(const ::capnp::List<ChartData::Area>::Reader &areas,
                                 const RenderConfig &renderConfig,
                                 QPainter *painter)
{
    painter->setBrush(Qt::transparent);
    painter->setCompositionMode(QPainter::CompositionMode_Clear);
    painter->setPen(Qt::NoPen);

    QPainterPath painterPath;

    // First define a polygon
    QPolygonF boundingPolygon(QRectF({ 0, 0 }, renderConfig.size));
    painterPath.addPolygon(boundingPolygon);

    for (const auto &area : areas) {
        auto polygons = area.getPolygons();
        for (const auto &polygon : polygons) {
            QVector<QPointF> points;

            // Reverse loop to define the hole to not clear.
            for (int i = polygon.getPositions().size() - 1; i >= 0; i--) {
                auto position = polygon.getPositions()[i];
                const QPointF pos = toMercator(renderConfig.topLeft,
                                               renderConfig.pixelsPerLongitude,
                                               position.getLatitude(),
                                               position.getLongitude())
                    + renderConfig.offset;
                points.append(pos);
            }
            painterPath.addPolygon(QPolygonF(points));
            painterPath.closeSubpath();
        }
    }

    painter->drawPath(painterPath);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
}

void MapTile::rasterClip(const QRectF &outer, const QRectF &inner, QPainter *painter)
{
    painter->setBrush(Qt::transparent);
    painter->setCompositionMode(QPainter::CompositionMode_Clear);
    painter->setPen(Qt::NoPen);

    QPainterPath path;
    path.addPolygon(QPolygonF(outer));
    QPolygonF polygon;

    // Counter-clockwise to define hole
    polygon.append(inner.topRight());
    polygon.append(inner.bottomRight());
    polygon.append(inner.bottomLeft());
    polygon.append(inner.topLeft());

    path.addPolygon(polygon);
    painter->drawPath(path);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
}

void MapTile::paintLandArea(const ::capnp::List<ChartData::LandArea>::Reader &areas,
                            const RenderConfig &renderConfig,
                            QPainter *painter)
{
    painter->setBrush(landColor);
    QPen pen(landEdgeColor);

    if (renderConfig.pixelsPerLongitude < 5000) {
        pen.setWidth(1);
    } else {
        pen.setWidth(2);
    }
    painter->setPen(pen);

    for (const auto &area : areas) {
        QPainterPath painterPath;
        auto polygons = area.getPolygons();
        for (const auto &polygon : polygons) {
            QVector<QPointF> points;
            for (const auto &position : polygon.getPositions()) {
                const QPointF pos = toMercator(renderConfig.topLeft,
                                               renderConfig.pixelsPerLongitude,
                                               position.getLatitude(),
                                               position.getLongitude())
                    + renderConfig.offset;
                points.append(pos);
            }

            painterPath.addPolygon(QPolygonF(points));
            painterPath.closeSubpath();
        }
        painter->drawPath(painterPath);
    }
}

QColor MapTile::depthColor(qreal depth)
{
    if (depth < 0.5) {
        return criticalWaterColor;
    } else if (depth < 3) {
        return shallowWaterColor;
    } else if (depth < 8) {
        return safetyWaterColor;
    }

    return QColor();
}

void MapTile::paintDepthAreas(const ::capnp::List<ChartData::DepthArea>::Reader &depthAreas,
                              const RenderConfig &renderConfig,
                              QPainter *painter)
{
    QPen pen;
    pen.setWidth(1);

    for (const auto &depthArea : depthAreas) {
        const QColor color = depthColor(depthArea.getDepth());
        QPainterPath painterPath;
        for (const auto &polygon : depthArea.getPolygons()) {
            QVector<QPointF> points;
            for (const auto &position : polygon.getPositions()) {
                QPointF point = toMercator(renderConfig.topLeft,
                                           renderConfig.pixelsPerLongitude,
                                           position.getLatitude(),
                                           position.getLongitude())
                    + renderConfig.offset;
                points.append(point);
            }
            painterPath.addPolygon(QPolygonF(points));
            painterPath.closeSubpath();
        }
        if (color.isValid()) {
            painter->setBrush(QBrush(color));
            pen.setColor(color);
        } else if (renderConfig.pixelsPerLongitude > 1000) { // Reduce the clutter when zoomed out a lot
            painter->setBrush(Qt::NoBrush);
            pen.setColor(QColor("#9bd1ff"));
        } else {
            continue;
        }

        painter->setPen(pen);
        painter->drawPath(painterPath);
    }
}

void MapTile::paintRoads(const capnp::List<ChartData::Road>::Reader &roads,
                         const RenderConfig &renderConfig,
                         QPainter *painter)
{

    painter->setBrush(Qt::NoBrush);
    QPen pen(QBrush("#c7c7a5"), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    for (const auto &road : roads) {
        QPainterPath painterPath;

        for (const auto &line : road.getLines()) {

            QVector<QPointF> points;
            for (const auto &pos : line.getPositions()) {
                QPointF point = toMercator(renderConfig.topLeft,
                                           renderConfig.pixelsPerLongitude,
                                           pos.getLatitude(),
                                           pos.getLongitude())
                    + renderConfig.offset;
                points.append(point);
            }
            painterPath.addPolygon(QPolygonF(points));
        }

        if (renderConfig.pixelsPerLongitude < 5000) {
            pen.setWidth(1);
        } else {
            switch (road.getCategory()) {
            case ::ChartData::CategoryOfRoad::MOTORWAY:
                pen.setWidth(4);
                break;

            case ::ChartData::CategoryOfRoad::MAJOR_ROAD:
                pen.setWidth(3);
                break;

            case ::ChartData::CategoryOfRoad::MINOR_ROAD:
                pen.setWidth(2);
                break;

            default:
                pen.setWidth(1);
                break;
            }
        }

        painter->setPen(pen);
        painter->drawPath(painterPath);
    }
}

void MapTile::paintBuiltUpArea(const ::capnp::List<ChartData::BuiltUpArea>::Reader &areas,
                               const RenderConfig &renderConfig,
                               QPainter *painter)
{
    QBrush brush(builtUpAreaColor);
    painter->setBrush(brush);
    painter->setPen(Qt::NoPen);

    for (const auto &area : areas) {
        QPainterPath painterPath;
        for (const auto &polygon : area.getPolygons()) {
            QVector<QPointF> points;
            for (const auto &position : polygon.getPositions()) {
                points << toMercator(renderConfig.topLeft,
                                     renderConfig.pixelsPerLongitude,
                                     position.getLatitude(),
                                     position.getLongitude())
                        + renderConfig.offset;
            }
            painterPath.addPolygon(QPolygonF(points));
            painterPath.closeSubpath();
        }
        painter->drawPath(painterPath);
    }
}

const QVariantMap &MapTile::tileRef() const
{
    return m_tileRef;
}

void MapTile::setTileRef(const QVariantMap &tileRef)
{
    m_tileRef = tileRef;
    emit tileRefChanged();

    bool ok;
    const double topLatitude = tileRef["topLatitude"].toDouble(&ok);
    if (!ok) {
        qWarning() << "Failed to convert topLatitude";
        return;
    }
    const double leftLongitude = tileRef["leftLongitude"].toDouble(&ok);
    if (!ok) {
        qWarning() << "Failed to convert leftLongitude";
        return;
    }
    const double bottomLatitude = tileRef["bottomLatitude"].toDouble(&ok);
    if (!ok) {
        qWarning() << "Failed to convert bottomLatitude";
        return;
    }
    const double rightLongitude = tileRef["rightLongitude"].toDouble(&ok);
    if (!ok) {
        qWarning() << "Failed to convert rightLongitude";
        return;
    }
    const int maxPixelsPerLongitude = tileRef["maxPixelsPerLongitude"].toInt(&ok);
    if (!ok) {
        qWarning() << "Failed to convert maxPixelsPerLongitude";
        return;
    }

    m_boundingBox = GeoRect(topLatitude, bottomLatitude, leftLongitude, rightLongitude);
    m_maxPixelsPerLongitude = maxPixelsPerLongitude;
    m_tileId = tileRef["tileId"].toString();

    // Used to trigger re-rendering when selecting a custom set of charts in the
    // debug view mode.
    m_updatedViewport = QVector3D();

    updateGeometry();
}

TileFactoryWrapper *MapTile::tileFactory() const
{
    return m_tileFactory;
}

void MapTile::setTileFactory(TileFactoryWrapper *newTileFactory)
{
    if (newTileFactory == m_tileFactory) {
        return;
    }
    m_tileFactory = newTileFactory;
    emit tileFactoryChanged(newTileFactory);
}

bool MapTile::loading() const
{
    return m_loading;
}

int MapTile::margin() const
{
    return tilePadding;
}


bool MapTile::error() const
{
    return m_error;
}
