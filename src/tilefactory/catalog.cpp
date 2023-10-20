#include <assert.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <oesenc/chartfile.h>
#include <oesenc/keylistreader.h>
#include <oesenc/serverreader.h>
#include <regex>

#include "tilefactory/catalog.h"

using namespace std;

namespace {

vector<filesystem::path> listFilesWithExtension(string_view dir, string_view extension)
{
    vector<filesystem::path> list;
    for (const filesystem::path &entry : std::filesystem::directory_iterator(dir)) {
        if (entry.extension() == extension) {
            list.push_back(entry);
        }
    }
    return list;
}

Catalog::Type detectCatalogType(oesenc::ServerControl *serverControl, string_view &dir)
{
    assert(serverControl);

    if (!serverControl->isReady()) {
        return Catalog::Type::Unknown;
    }

    vector<filesystem::path> oesencFiles = listFilesWithExtension(dir, ".oesenc");

    if (!oesencFiles.empty()) {
        auto key = oesenc::KeyListReader::readOesencKey(dir);
        auto stream = oesenc::ServerReader::openOesenc(serverControl->pipeName(),
                                                       (dir / oesencFiles.front()).string(),
                                                       key);
        oesenc::ChartFile chartFile(*stream);
        if (chartFile.readHeaders()) {
            return Catalog::Type::Oesenc;
        }
    }

    vector<filesystem::path> oesuFiles = listFilesWithExtension(dir, ".oesu");

    if (!oesuFiles.empty()) {
        auto keys = oesenc::KeyListReader::readOesuKeys(dir);
        std::string baseName = oesuFiles.front().stem().string();
        if (keys.find(baseName) == keys.end()) {
            return Catalog::Type::Unknown;
        }
        auto stream = oesenc::ServerReader::openOesu(serverControl->pipeName(),
                                                     (dir / oesuFiles.front()).string(),
                                                     keys[baseName]);
        oesenc::ChartFile chartFile(*stream);
        if (chartFile.readHeaders()) {
            return Catalog::Type::Oesu;
        }
    }

    vector<filesystem::path> bothFiles = oesencFiles;
    bothFiles.insert(bothFiles.begin(), oesuFiles.begin(), oesuFiles.end());

    if (!bothFiles.empty()) {
        auto stream = std::ifstream(bothFiles.front(), std::ios::binary);
        oesenc::ChartFile chartFile(stream);

        if (chartFile.readHeaders()) {
            return Catalog::Type::Decrypted;
        }
    }
    return Catalog::Type::Unknown;
}
}

Catalog::Catalog(oesenc::ServerControl *serverControl, string_view dir)
    : m_dir(dir)
    , m_serverControl(serverControl)
{
    assert(serverControl != nullptr);
    m_oesuKeys = oesenc::KeyListReader::readOesuKeys(dir);
    m_oesencKey = oesenc::KeyListReader::readOesencKey(dir);
    m_type = detectCatalogType(m_serverControl, dir);
}

Catalog::Type Catalog::type() const
{
    return m_type;
}

vector<string> Catalog::chartFileNames() const
{
    vector<filesystem::path> oesencFiles = listFilesWithExtension(m_dir.string(), ".oesenc");
    vector<filesystem::path> oesuFiles = listFilesWithExtension(m_dir.string(), ".oesu");

    vector<filesystem::path> bothFiles = oesencFiles;
    bothFiles.insert(bothFiles.begin(), oesuFiles.begin(), oesuFiles.end());

    vector<string> names;
    names.resize(bothFiles.size());

    std::transform(bothFiles.cbegin(), bothFiles.cend(), names.begin(), [](const filesystem::path &path) {
        return path.filename().string();
    });
    return names;
}

std::shared_ptr<std::istream> Catalog::openChart(std::string_view fileName)
{
    // The user of this class should ensure that there is only one std::istream
    // reading from the oexserverd process.
    //
    // The following expression asserts if that rule is broken
    assert(m_currentStream.use_count() <= 1);

    filesystem::path filePath = m_dir / std::string(fileName);
    string fileNameWithoutExtension = filePath.stem().string();

    switch (m_type) {
    case Type::Oesu:

        if (m_oesuKeys.find(fileNameWithoutExtension) == m_oesuKeys.end()) {
            return nullptr;
        }
        m_currentStream = oesenc::ServerReader::openOesu(m_serverControl->pipeName(),
                                                         filePath.string(),
                                                         m_oesuKeys[fileNameWithoutExtension]);
        return m_currentStream;
    case Type::Oesenc:
        m_currentStream = oesenc::ServerReader::openOesenc(m_serverControl->pipeName(), filePath.string(), m_oesencKey);
        return m_currentStream;
    case Type::Decrypted:
        return make_shared<ifstream>(filePath, std::ios::binary);
    default:
        return nullptr;
    }

    return nullptr;
}
