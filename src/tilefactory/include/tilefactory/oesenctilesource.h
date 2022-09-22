#pragma once

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "itilesource.h"
#include "oesenc/chartfile.h"
#include "tilefactory/chart.h"

class OesencTileSource : public ITileSource
{
public:
    OesencTileSource(const std::string &chartFile,
                     const std::string &name,
                     const std::string &tileDir);
    OesencTileSource(const std::vector<std::byte> &data,
                     const std::string &originalFile,
                     const std::string &tileDir);
    bool isValid() const;
    ~OesencTileSource();
    GeoRect extent() const override;
    int scale() const override { return m_scale; }
    std::shared_ptr<Chart> create(const GeoRect &boundingBox, int pixelsPerLongitude) override;

private:
    void ensureTileDir(const std::string &tileDir);

    /*!
        Creates a unique tile identification string for a given boundingBox zoom
    */
    static std::string tileId(const GeoRect &boundingBox, int pixelsPerLongitude);

    /*!
        Generate tile data for the given boundingBox

        The actual ChartFile will not be opened until the first call to this function
    */
    std::shared_ptr<Chart> generateTile(const GeoRect &boundingBox, int pixelsPerLongitude);
    Chart::SoundingCache m_soundingCache;
    std::unique_ptr<Chart> m_entireChart;
    std::unordered_map<std::string, std::shared_ptr<std::mutex>> m_tileMutexes;
    std::mutex m_tileMutexesMutex;
    std::string m_tileDir;
    std::string m_file;
    std::string m_name;
    oesenc::ChartFile m_oesencChart;
    oesenc::Rect m_extent;
    std::mutex m_chartMutex;
    int m_scale = 0;
    bool m_valid = false;
};
