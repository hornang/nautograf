#include <filesystem>
#include <random>
#include <sstream>

#include "filehelper.h"

std::string FileHelper::tileId(const GeoRect &boundingBox, int pixelsPerLongitude)
{
    std::stringstream ss;
    ss << boundingBox.top();
    ss << boundingBox.left();
    ss << boundingBox.bottom();
    ss << boundingBox.right();
    ss << pixelsPerLongitude;
    unsigned int h1 = std::hash<std::string> {}(ss.str());

    static const std::string_view hex_chars = "0123456789abcdef";

    std::mt19937 mt(h1);

    std::string uuid;
    unsigned int len = 16;
    uuid.reserve(len);

    while (uuid.size() < len) {
        auto n = mt();
        for (auto i = std::mt19937::max(); i & 0x8 && uuid.size() < len; i >>= 4) {
            uuid += hex_chars[n & 0xf];
            n >>= 4;
        }
    }

    return uuid;
}

std::string FileHelper::getTileDir(const std::string &tileDir, uint64_t typeId)
{
    std::stringstream ss;
    ss << std::hex << typeId;
    std::string s = ss.str();
    std::filesystem::path dir;
    dir.append(tileDir);
    dir.append(ss.str());
    std::filesystem::create_directories(dir);
    return dir.string();
}

std::string FileHelper::tileFileName(const std::string &tileDir,
                                     const std::string &name,
                                     const std::string &id)
{
    std::filesystem::path path(tileDir);
    return (path / name / (id + std::string(".bin"))).string();
}

std::string FileHelper::internalChartFileName(const std::string &tileDir,
                                              const std::string &name,
                                              int pixelsPerLon)
{
    std::filesystem::path path(tileDir);
    std::stringstream ss;
    ss << pixelsPerLon;
    std::string baseName = "all_" + ss.str() + ".bin";
    return (path / name / baseName).string();
}
