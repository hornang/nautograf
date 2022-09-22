#pragma once

#include <QHash>
#include <QImage>

#include "chartdata.capnp.h"

class SymbolImage
{
public:
    struct TextureSymbol
    {
        QRectF coords;
        QSize size;

        // This is the pixel position within the symbol that should be placed on
        // geographical position in the map. E.g for a stake the bottom of the
        // symbol is to be placed, not the center.
        QPointF center;

        // Rectangle within the symbol which should not be overlapped
        // For many symbols it is simply QRectF(0, 0, size.width(), size.height())
        QRectF roi;
        QColor color;

        bool operator!=(const TextureSymbol &other) const
        {
            return (this->coords != other.coords || this->size != other.size);
        }

        bool operator==(const TextureSymbol &other) const
        {
            return (this->coords == other.coords && this->size == other.size);
        }
    };

    SymbolImage(const QString &baseDir);
    SymbolImage(const SymbolImage &other) = delete;
    bool isInitialized() const { return m_initialized; }
    void load();
    const QImage image() const { return m_image; }
    std::optional<TextureSymbol> underwaterRock(const ChartData::UnderwaterRock::Reader &rock) const;
    std::optional<TextureSymbol> lateralBuoy(const ChartData::BuoyLateral::Reader &buoy) const;
    std::optional<TextureSymbol> beacon(const ChartData::Beacon::Reader &beacon) const;
    std::optional<SymbolImage::TextureSymbol> testSymbol() const;

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
    QPoint m_position;
    QString m_baseDir;
    bool m_initialized = false;
};
