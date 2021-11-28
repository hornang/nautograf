#pragma once

#include <QHash>
#include <QImage>
#include <QObject>

#include "chartdata.capnp.h"

static constexpr int maxSymbolsPerType = 20;

class ChartSymbols : public QObject
{
    Q_OBJECT
public:
    ChartSymbols(const QString &baseDir);
    ChartSymbols(const ChartSymbols &other) = delete;
    void operator=(ChartSymbols const &) = delete;
    QImage beacon(ChartData::BeaconShape shape);
    QImage underwaterRock(ChartData::WaterLevelEffect waterLevelEffect);
    QImage lateralBuoy(ChartData::BuoyShape shape,
                       ChartData::CategoryOfLateralMark category,
                       ChartData::Color color);
    static QString colorAcronym(ChartData::Color color);

private:
    template <typename T, std::array<std::pair<T, const char *>, maxSymbolsPerType> *files>
    QImage getSymbol(T variant, QHash<T, QImage> &hash);

    /*!
     * Creates a unique key for a given shape and color
     */
    static QString buoykey(ChartData::BuoyShape shape, const QColor &color);

    static QImage render(const QString &file, const QColor &color = QColor());
    QHash<ChartData::BeaconShape, QImage> m_beacons;
    QHash<ChartData::WaterLevelEffect, QImage> m_underwaterRocks;
    QHash<QString, QImage> m_lateralBuoys;
    QString m_baseDir;
};
