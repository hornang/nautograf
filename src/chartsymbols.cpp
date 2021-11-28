#include <QDir>
#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

#include "chartsymbols.h"

static const QHash<ChartData::Color, QString> colorAcronyms { { ChartData::Color::RED, "R" },
                                                              { ChartData::Color::GREEN, "G" } };
struct BuoySymbol
{
    ChartData::BuoyShape shape;
    const char *port;
    const char *starboard;
};

// Let's try to initialize stuff without static intialization code:
static const BuoySymbol buoyFiles[] = {
    { ChartData::BuoyShape::CONICAL, "buoys/conical.svg", "buoys/conical.svg" },
    { ChartData::BuoyShape::CAN, "buoys/can.svg", "buoys/can.svg" },
    { ChartData::BuoyShape::SPHERICAL, "buoys/spherical.svg", "buoys/spherical.svg" },
    { ChartData::BuoyShape::PILLAR, "buoys/pillar.svg", "buoys/pillar.svg" },
    { ChartData::BuoyShape::SPAR, "buoys/spar_port.svg", "buoys/spar_starboard.svg" },
    { ChartData::BuoyShape::BARREL, "buoys/barrel.svg", "buoys/barrel.svg" },
    { ChartData::BuoyShape::SUPER_BUOY, "buoys/super_buoy.svg", "buoys/super_buoy.svg" }
};

// Use the the Norwegian "båke" symbol for every shape
static std::array<std::pair<ChartData::BeaconShape, const char *>, maxSymbolsPerType> beaconFiles = {
    std::pair(ChartData::BeaconShape::STAKE, "beacons/beacon_norwegian.svg"),
    std::pair(ChartData::BeaconShape::BEACON_TOWER, "beacons/beacon_norwegian.svg"),
    std::pair(ChartData::BeaconShape::LATTICE_BEACON, "beacons/beacon_norwegian.svg"),
    std::pair(ChartData::BeaconShape::PILE_BEACON, "beacons/beacon_norwegian.svg"),
    std::pair(ChartData::BeaconShape::CAIRN, "beacons/beacon_norwegian.svg"),
    std::pair(ChartData::BeaconShape::BOYANT, "beacons/beacon_norwegian.svg"),
};

static std::array<std::pair<ChartData::WaterLevelEffect, const char *>, maxSymbolsPerType> underwaterRocks = {
    std::pair(ChartData::WaterLevelEffect::AWASH, "underwater_rocks/awash.svg"),
    std::pair(ChartData::WaterLevelEffect::COVERS_AND_UNCOVERS, "underwater_rocks/cover_and_uncovers.svg"),
    std::pair(ChartData::WaterLevelEffect::ALWAYS_SUBMERGED, "underwater_rocks/always_submerged.svg")
};

ChartSymbols::ChartSymbols(const QString &baseDir)
    : m_baseDir(baseDir)
{
}

QString ChartSymbols::buoykey(ChartData::BuoyShape shape, const QColor &color)
{
    return QString::number(static_cast<int>(shape))
        + QString::number(color.red())
        + QString::number(color.green())
        + QString::number(color.blue());
}

QString ChartSymbols::colorAcronym(ChartData::Color color)
{
    return colorAcronyms.value(color);
}

QImage ChartSymbols::underwaterRock(ChartData::WaterLevelEffect waterLevelEffect)
{
    return getSymbol<ChartData::WaterLevelEffect, &underwaterRocks>(waterLevelEffect, m_underwaterRocks);
}

QImage ChartSymbols::beacon(ChartData::BeaconShape shape)
{
    return getSymbol<ChartData::BeaconShape, &beaconFiles>(shape, m_beacons);
}

template <typename T, std::array<std::pair<T, const char *>, maxSymbolsPerType> *files>
QImage ChartSymbols::getSymbol(T variant, QHash<T, QImage> &hash)
{
    if (!hash.contains(variant)) {
        for (const auto &item : *files) {
            if (item.first == variant) {
                hash[variant] = render(m_baseDir + "/" + item.second, 30);
                return hash[variant];
            }
        }
        qWarning() << "No symbol for object variant" << static_cast<int>(variant);
    }
    return hash[variant];
}

QImage ChartSymbols::lateralBuoy(ChartData::BuoyShape shape,
                                 ChartData::CategoryOfLateralMark category,
                                 ChartData::Color colorType)
{
    QColor color;

    if (colorType == ChartData::Color::GREEN) {
        color = QColor(0, 170, 0);
    } else if (colorType == ChartData::Color::RED) {
        color = Qt::red;
    }

    QString key = buoykey(shape, color);

    if (!m_lateralBuoys.contains(key)) {
        for (const auto &item : buoyFiles) {
            if (item.shape == shape) {
                QString file;
                if (category == ChartData::CategoryOfLateralMark::PORT
                    || category == ChartData::CategoryOfLateralMark::CHANNEL_TO_STARBOARD) {
                    file = item.port;
                } else if (category == ChartData::CategoryOfLateralMark::STARBOARD
                           || category == ChartData::CategoryOfLateralMark::CHANNEL_TO_PORT) {
                    file = item.starboard;
                }

                QImage image;
                image = render(m_baseDir + "/" + file, color);
                m_lateralBuoys[key] = image;
                break;
            }
        }
    }
    return m_lateralBuoys[key];
}

QImage ChartSymbols::render(const QString &filename, const QColor &color)
{
    QSvgRenderer renderer;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Unable to open SVG" << filename;
        return QImage();
    }
    QString content = file.readAll();

    if (color.isValid()) {
        QString hex = QString("#%1%2%3").arg(color.red(), 2, 16, QChar('0')).arg(color.green(), 2, 16, QChar('0')).arg(color.blue(), 2, 16, QChar('0'));
        content = content.replace("fill:#000000", "fill:" + hex);
        content = content.replace("stroke:#000000", "stroke:" + hex);
    }

    if (!renderer.load(content.toUtf8())) {
        return QImage();
    }

    QImage image(renderer.defaultSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor("transparent"));
    QPainter painter(&image);
    renderer.render(&painter);
    return image;
}
