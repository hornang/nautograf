#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include <oesenc/servercontrol.h>

#include "tilefactory_export.h"

class TILEFACTORY_EXPORT Catalog
{
public:
    enum class Type {
        Unknown,
        Oesu,
        Oesenc,
        Decrypted,
    };

    Catalog(oesenc::ServerControl *serverControl, std::string_view dir);
    std::shared_ptr<std::istream> openChart(std::string_view fileName);
    std::vector<std::string> chartFileNames() const;
    Type type() const;

private:
    std::unordered_map<std::string, std::string> m_oesuKeys;
    std::string m_oesencKey;
    std::filesystem::path m_dir;
    Type m_type = Type::Unknown;
    oesenc::ServerControl *m_serverControl;
    std::shared_ptr<std::istream> m_currentStream;
};
