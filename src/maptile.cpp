#include <QFontMetrics>
#include <QList>
#include <QThread>
#include <QVector>
#include <QtConcurrent>
#include <algorithm>

#include "maptile.h"
#include "scene/tilefactorywrapper.h"
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

void MapTile::setChartVisibility(const QString &name, bool visible)
{
    if (visible && m_hiddenCharts.contains(name)) {
        m_hiddenCharts.removeAll(name);
    } else if (!visible && !m_hiddenCharts.contains(name)) {
        m_hiddenCharts.append(name);
    } else {
        return;
    }
    render(m_lat, m_lon, m_pixelsPerLon);
}

void MapTile::render(double lat, double lon, double pixelsPerLon)
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
    renderConfig.pixelsPerLongitude = pixelsPerLon;

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
        render(m_lat, m_lon, m_pixelsPerLon);
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

QSizeF MapTile::sizeFromPixelsPerLon(const GeoRect &boundingBox, double pixelsPerLon)
{
    if (boundingBox.isNull()) {
        return QSize();
    }

    double width = Mercator::mercatorWidth(boundingBox.left(),
                                           boundingBox.right(),
                                           pixelsPerLon);

    double height = Mercator::mercatorHeight(boundingBox.top(),
                                             boundingBox.bottom(),
                                             pixelsPerLon);
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
    if ((m_updatedPixelsPerLon == m_pixelsPerLon && m_updatedLat == m_lat && m_updatedLon == m_lon)
        || m_boundingBox.isNull()) {
        return;
    }

    if (m_boundingBox.isNull()) {
        return;
    }

    double x = Mercator::mercatorWidth(m_lon, m_boundingBox.left(), m_pixelsPerLon);
    double y = Mercator::mercatorHeight(m_lat, m_boundingBox.top(), m_pixelsPerLon);
    QPointF position(x - tilePadding, y - tilePadding);

    if (m_updatedPixelsPerLon != m_pixelsPerLon) {
        const QSizeF content = sizeFromPixelsPerLon(m_boundingBox, m_pixelsPerLon);
        m_imageSize = content.toSize() + extraPixels;
        const QSizeF size = QSize(content.width() + 2. * tilePadding,
                                  content.height() + 2. * tilePadding);

        setPosition(position);
        setSize(size);
        render(m_lat, m_lon, m_pixelsPerLon);
    } else {
        setPosition(position);
    }

    m_updatedLat = m_lat;
    m_updatedLon = m_lon;
    m_updatedPixelsPerLon = m_pixelsPerLon;
}

double MapTile::lat() const
{
    return m_lat;
}

void MapTile::setLat(double newLat)
{
    if (qFuzzyCompare(m_lat, newLat))
        return;
    m_lat = newLat;
    emit latChanged();
    updateGeometry();
}

double MapTile::lon() const
{
    return m_lon;
}

void MapTile::setLon(double newLon)
{
    if (qFuzzyCompare(m_lon, newLon))
        return;
    m_lon = newLon;
    emit lonChanged();
    updateGeometry();
}

double MapTile::pixelsPerLon() const
{
    return m_pixelsPerLon;
}

void MapTile::setPixelsPerLon(double newPixelsPerLon)
{
    if (qFuzzyCompare(m_pixelsPerLon, newPixelsPerLon))
        return;
    m_pixelsPerLon = newPixelsPerLon;
    emit pixelsPerLonChanged();
    updateGeometry();
}

void MapTile::paint(QPainter *painter)
{
    RenderConfig renderConfig;
    renderConfig.offset = QPointF(extraPixels.width(), extraPixels.height() / 2);
    renderConfig.topLeft = m_boundingBox.topLeft();
    renderConfig.size = m_imageSize;
    renderConfig.pixelsPerLongitude = m_pixelsPerLon;

    const QRectF targetRect = QRectF(tilePadding - extraPixels.width() / 2,
                                     tilePadding - extraPixels.height() / 2,
                                     width() - 2 * tilePadding + extraPixels.width(),
                                     height() - 2 * tilePadding + extraPixels.height());

    if (!m_image.isNull()) {
        const QRectF sourceRect(QPointF(0, 0), m_image.size());
        painter->drawImage(targetRect, m_image, sourceRect);
    }
}

QPolygonF MapTile::fromCapnpToPolygon(const capnp::List<ChartData::Position>::Reader src,
                                      const RenderConfig &renderConfig)
{
    QVector<QPointF> points;
    for (const auto &position : src) {
        const QPointF pos = toMercator(renderConfig.topLeft,
                                       renderConfig.pixelsPerLongitude,
                                       position.getLatitude(),
                                       position.getLongitude())
            + renderConfig.offset;
        points.append(pos);
    }

    return QPolygonF(points);
}

QPolygonF MapTile::reversePolygon(const QPolygonF &input)
{
    QList<QPointF> result;

    QList<QPointF>::const_reverse_iterator i;
    for (i = input.rbegin(); i != input.rend(); ++i) {
        result.append(*i);
    }

    return result;
}

void MapTile::clipAroundCoverage(const ::capnp::List<ChartData::CoverageArea>::Reader &areas,
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
            // Reverse loop to define the hole to not clear.
            painterPath.addPolygon(reversePolygon(fromCapnpToPolygon(polygon.getMain(), renderConfig)));
            painterPath.closeSubpath();

            // Holes in coverage are uncommon, but reversing the holes (which
            // then will be clipped out) should in theory work.
            for (const capnp::List<ChartData::Position>::Reader &hole : polygon.getHoles()) {
                painterPath.addPolygon(reversePolygon(fromCapnpToPolygon(hole, renderConfig)));
                painterPath.closeSubpath();
            }
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
            painterPath.addPolygon(fromCapnpToPolygon(polygon.getMain(), renderConfig));
            painterPath.closeSubpath();

            for (const capnp::List<ChartData::Position>::Reader &hole : polygon.getHoles()) {
                painterPath.addPolygon(fromCapnpToPolygon(hole, renderConfig));
                painterPath.closeSubpath();
            }
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
            painterPath.addPolygon(fromCapnpToPolygon(polygon.getMain(), renderConfig));
            painterPath.closeSubpath();

            for (const capnp::List<ChartData::Position>::Reader &hole : polygon.getHoles()) {
                painterPath.addPolygon(fromCapnpToPolygon(hole, renderConfig));
                painterPath.closeSubpath();
            }
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
            painterPath.addPolygon(fromCapnpToPolygon(polygon.getMain(), renderConfig));
            painterPath.closeSubpath();

            for (const capnp::List<ChartData::Position>::Reader &hole : polygon.getHoles()) {
                painterPath.addPolygon(fromCapnpToPolygon(hole, renderConfig));
                painterPath.closeSubpath();
            }
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
    m_updatedPixelsPerLon = 0;
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
