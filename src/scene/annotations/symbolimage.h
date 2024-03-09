#pragma once

#include <QHash>
#include <QImage>

#include "chartdata.capnp.h"
#include "scene/annotations/types.h"

class SymbolImage
{
public:
    SymbolImage(const QString &baseDir);
    SymbolImage(const SymbolImage &other) = delete;
    void load();
    const QImage image() const { return m_image; }
    std::optional<TextureSymbol> underwaterRock(const ChartData::UnderwaterRock::Reader &rock) const;
    std::optional<TextureSymbol> lateralBuoy(const ChartData::BuoyLateral::Reader &buoy) const;
    std::optional<TextureSymbol> beacon(const ChartData::Beacon::Reader &beacon) const;
    std::optional<TextureSymbol> testSymbol() const;

private:
    enum class Laterality {
        Port,
        Starboard
    };

    static QString underwaterRockHash(ChartData::WaterLevelEffect waterLevelEffect);
    static QString buoyHash(ChartData::BuoyShape buoyShape, Laterality laterality);
    static QString beaconHash(ChartData::BeaconShape shape);
    static QRectF toNormalized(const QRect &rect, const QSize &image);
    QRect render(QImage &image,
                 const QRect &lastRect,
                 int &rowHeight,
                 const QString &filename);

    QHash<QString, TextureSymbol> m_symbols;
    QImage m_image;
    QString m_baseDir;
};
