#pragma once

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "itilesource.h"
#include "oesenc/chartfile.h"
#include "tilefactory/chart.h"

#include "tilefactory_export.h"

class Catalog;

class TILEFACTORY_EXPORT OesencTileSource : public ITileSource
{
public:
    OesencTileSource(Catalog *catalogue,
                     std::string_view name,
                     std::string_view baseTileDir);

    bool isValid() const;
    ~OesencTileSource();
    GeoRect extent() const override;
    int scale() const override { return m_scale; }
    std::shared_ptr<Chart> create(const GeoRect &boundingBox, int pixelsPerLongitude) override;

private:
    bool convertChartToInternalFormat(float lineEpsilon, int pixelsPerLon);
    void readOesencMetaData(const oesenc::ChartFile *chart);
    static GeoRect fromOesencRect(const oesenc::Rect &src);

    /*!
        Generate tile data for the given boundingBox

        The actual ChartFile will not be opened until the first call to this function
    */
    std::shared_ptr<Chart> generateTile(const GeoRect &boundingBox, int pixelsPerLongitude);
    std::unordered_map<std::string, std::shared_ptr<std::mutex>> m_tileMutexes;
    std::mutex m_tileMutexesMutex;
    std::string m_tileDir;
    std::string m_name;
    bool m_valid = false;
    GeoRect m_extent;
    std::mutex m_internalChartMutex;
    Catalog *m_catalogue = nullptr;
    int m_scale = 0;
};
