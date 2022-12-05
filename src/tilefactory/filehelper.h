#pragma once

#include <string>

#include "tilefactory/georect.h"

class FileHelper
{
public:
    static std::string tileId(const GeoRect &boundingBox, int pixelsPerLongitude);
    static std::string chartTypeIdToString(uint64_t typeId);
    static std::string getTileDir(const std::string &tileDir, uint64_t typeId);
    static std::string tileFileName(const std::string &tileDir,
                                    const std::string &name,
                                    const std::string &id);
    static std::string internalChartFileName(const std::string &tileDir,
                                             const std::string &name);
};
