#include <QDebug>
#include <QFile>

#include <ext/import-svg.h>
#include <msdfgen.h>

#include "symbolimage.h"

const int imageWidth = 300;
const int imageHeight = 300;

SymbolImage::SymbolImage(const QString &baseDir)
    : m_baseDir(baseDir)
{
    load();
}

void SymbolImage::load()
{
    m_image = QImage(QSize(imageWidth, imageHeight), QImage::Format_Grayscale8);
    m_image.fill(QColor("black"));

    QRect rect;
    int rowHeight = 0;

    auto testSymbolRect = render(m_image, rect, rowHeight, m_baseDir + "/test_symbol.svg");
    if (!testSymbolRect.isNull()) {
        rect = testSymbolRect;
        m_symbols["test"] = TextureSymbol { toNormalized(rect, m_image.size()),
                                            rect.size(),
                                            QPointF(rect.width() / 2, rect.height() / 2),
                                            QRectF(0, 0, rect.width(), rect.height()),
                                            QColor(0, 0, 0) };
    }

    QList<QPair<ChartData::WaterLevelEffect, QString>> rocks = {
        { ChartData::WaterLevelEffect::AWASH, "underwater_rocks/awash.svg" },
        { ChartData::WaterLevelEffect::COVERS_AND_UNCOVERS, "underwater_rocks/cover_and_uncovers.svg" },
        { ChartData::WaterLevelEffect::ALWAYS_SUBMERGED, "underwater_rocks/always_submerged.svg" }
    };

    for (const auto &rock : rocks) {
        auto newRect = render(m_image, rect, rowHeight, m_baseDir + "/" + rock.second);
        if (!newRect.isNull()) {
            rect = newRect;
            QPointF center(rect.width() / 2, rect.height() / 2);
            m_symbols[underwaterRockHash(rock.first)] = TextureSymbol { toNormalized(rect, m_image.size()),
                                                                        rect.size(),
                                                                        center,
                                                                        QRectF(0, 0, rect.width(), rect.height()),
                                                                        QColor(0, 0, 0) };
        }
    }

    struct LateralBuoy
    {
        ChartData::BuoyShape shape;
        QString port;
        QString starboard;
    };

    QList<LateralBuoy> buoys = {
        { ChartData::BuoyShape::CONICAL, "buoys/conical.svg", "buoys/conical.svg" },
        { ChartData::BuoyShape::CAN, "buoys/can.svg", "buoys/can.svg" },
        { ChartData::BuoyShape::SPHERICAL, "buoys/spherical.svg", "buoys/spherical.svg" },
        { ChartData::BuoyShape::PILLAR, "buoys/pillar.svg", "buoys/pillar.svg" },
        { ChartData::BuoyShape::SPAR, "buoys/spar_port.svg", "buoys/spar_starboard.svg" },
        { ChartData::BuoyShape::BARREL, "buoys/barrel.svg", "buoys/barrel.svg" },
        { ChartData::BuoyShape::SUPER_BUOY, "buoys/super_buoy.svg", "buoys/super_buoy.svg" },
    };

    for (const auto &buoy : buoys) {
        auto newRect = render(m_image, rect, rowHeight, m_baseDir + "/" + buoy.port);
        if (!newRect.isNull()) {
            rect = newRect;
            QPointF center(rect.width() / 2, rect.height());
            QRectF roi(0, rect.size().height() * 3. / 4., rect.width(), rect.height() / 4.);
            m_symbols[buoyHash(buoy.shape, Laterality::Port)] = TextureSymbol { toNormalized(rect, m_image.size()),
                                                                                rect.size(),
                                                                                center,
                                                                                roi,
                                                                                QColor(220, 0, 0) };
        }
    }

    for (const auto &buoy : buoys) {
        auto newRect = render(m_image, rect, rowHeight, m_baseDir + "/" + buoy.starboard);
        if (!newRect.isNull()) {
            rect = newRect;
            QPointF center(rect.width() / 2, rect.height());
            QRectF roi(0, rect.size().height() * 3. / 4., rect.width(), rect.height() / 4.);
            m_symbols[buoyHash(buoy.shape, Laterality::Starboard)] = TextureSymbol { toNormalized(rect, m_image.size()),
                                                                                     rect.size(),
                                                                                     center,
                                                                                     roi,
                                                                                     QColor(0, 200, 0) };
        }
    }

    struct Beacon
    {
        ChartData::BeaconShape shape;
        QString svg;
    };

    // Use the the Norwegian "båke" symbol for every shape
    QList<Beacon> beacons = {
        { ChartData::BeaconShape::STAKE, "beacons/beacon_norwegian.svg" },
        { ChartData::BeaconShape::BEACON_TOWER, "beacons/beacon_norwegian.svg" },
        { ChartData::BeaconShape::LATTICE_BEACON, "beacons/beacon_norwegian.svg" },
        { ChartData::BeaconShape::PILE_BEACON, "beacons/beacon_norwegian.svg" },
        { ChartData::BeaconShape::CAIRN, "beacons/beacon_norwegian.svg" },
        { ChartData::BeaconShape::BOYANT, "beacons/beacon_norwegian.svg" },
    };

    for (const auto &beacon : beacons) {
        auto newRect = render(m_image, rect, rowHeight, m_baseDir + "/" + beacon.svg);
        if (!newRect.isNull()) {
            rect = newRect;
            m_symbols[beaconHash(beacon.shape)] = TextureSymbol { toNormalized(rect, m_image.size()),
                                                                  rect.size(),
                                                                  QPointF(rect.width() / 2, rect.height() / 2),
                                                                  QRectF(0, 0, rect.width(), rect.height()),
                                                                  QColor(0, 0, 0) };
        }
    }
}

QString SymbolImage::beaconHash(ChartData::BeaconShape shape)
{
    return QString::number((int)shape);
}

QRectF SymbolImage::toNormalized(const QRect &rect, const QSize &image)
{
    return QRectF((float)rect.left() / (float)image.width(), (float)rect.top() / image.height(),
                  (float)rect.width() / (float)image.width(), (float)rect.height() / image.height());
}

QString SymbolImage::underwaterRockHash(ChartData::WaterLevelEffect waterLevelEffect)
{
    return QString("rock_" + QString::number((int)waterLevelEffect));
}

QString SymbolImage::buoyHash(ChartData::BuoyShape buoyShape, SymbolImage::Laterality laterality)
{
    return QString("buoy" + QString::number((int)buoyShape + (int)laterality * 20));
}

std::optional<SymbolImage::TextureSymbol> SymbolImage::underwaterRock(const ChartData::UnderwaterRock::Reader &rock) const
{
    const auto hash = underwaterRockHash(rock.getWaterlevelEffect());

    if (!m_symbols.contains(hash)) {
        return {};
    }

    return m_symbols[hash];
}

std::optional<SymbolImage::TextureSymbol> SymbolImage::beacon(const ChartData::Beacon::Reader &beacon) const
{
    const auto hash = beaconHash(beacon.getShape());
    if (!m_symbols.contains(hash)) {
        return {};
    }

    return m_symbols[hash];
}

std::optional<SymbolImage::TextureSymbol> SymbolImage::lateralBuoy(const ChartData::BuoyLateral::Reader &buoy) const
{
    auto category = buoy.getCategory();

    Laterality laterality = Laterality::Port;
    switch (category) {
    case ChartData::CategoryOfLateralMark::PORT:
    case ChartData::CategoryOfLateralMark::CHANNEL_TO_PORT:
        laterality = Laterality::Port;
        break;
    case ChartData::CategoryOfLateralMark::STARBOARD:
    case ChartData::CategoryOfLateralMark::CHANNEL_TO_STARBOARD:
        laterality = Laterality::Starboard;
        break;
    default:
        return {};
    }

    const auto hash = buoyHash(buoy.getShape(), laterality);

    if (!m_symbols.contains(hash)) {
        return {};
    }

    return m_symbols[hash];
}

std::optional<SymbolImage::TextureSymbol> SymbolImage::testSymbol() const
{
    if (!m_symbols.contains("test")) {
        return {};
    }

    return m_symbols["test"];
}

QRect SymbolImage::render(QImage &image,
                          const QRect &lastRect,
                          int &rowHeight,
                          const QString &filename)
{

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Unable to open SVG" << filename;
        return {};
    }

    QByteArray content = file.readAll();

    msdfgen::Shape shape;
    msdfgen::loadSvgShape(shape, content.data(), content.size());
    shape.normalize();

    // This will result in a quite sharp edge. This seems to be required to
    // render thin objects with msdfgen.
    const int distanceRange = 1;
    auto bounds = shape.getBounds(distanceRange);

    float width = bounds.r - bounds.l;
    float height = bounds.t - bounds.b;

    if (bounds.r > 1024 || bounds.l > 1024) {
        qWarning() << "Failed to render" << filename;
        return {};
    }

    QSize size(width, height);
    QRect targetRect(lastRect.topRight(), size);
    targetRect.moveLeft(targetRect.left() + 10);

    if (targetRect.right() > image.width()) {
        targetRect.moveLeft(0);
        targetRect.moveTop(lastRect.top() + rowHeight);
    }

    if (targetRect.height() > rowHeight) {
        rowHeight = targetRect.height();
    }

    msdfgen::Bitmap<float, 1> msdf(size.width(), size.height());

    msdfgen::generateSDF(msdf, shape, distanceRange, 1.0, msdfgen::Vector2(-bounds.l, -bounds.b));

    for (int y = 0; y < size.height(); y++) {
        // Assumes 8 bit pixels
        uchar *scanLine = image.scanLine(targetRect.top() + y);
        for (int x = 0; x < size.width(); x++) {
            auto value = std::clamp<float>(*msdf(x, msdf.height() - y - 1), -1, 1);
            scanLine[targetRect.left() + x] = 127 + 127 * value;
        }
    }

    return targetRect;
}
