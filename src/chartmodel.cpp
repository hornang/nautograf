#include <QDir>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>
#include <chrono>

#include <oesenc/chartfile.h>
#include <oesenc/serverreader.h>

#include "chartmodel.h"
#include "tilefactory/catalog.h"
#include "tilefactory/oesenctilesource.h"
#include "tilefactory/tilefactory.h"
#include "usersettings.h"

namespace {
const QString chartDirKey = "ChartDir";
std::chrono::duration serverPollTimeout = std::chrono::seconds(5);
std::chrono::duration serverPollInterval = std::chrono::milliseconds(500);
}

ChartModel::ChartModel(std::shared_ptr<TileFactory> tileFactory)
    : m_tileFactory(tileFactory)
    , m_oesencServerControl(std::make_unique<oesenc::ServerControl>())
    , m_roleNames({
          { (int)Role::Name, "name" },
          { (int)Role::Scale, "scale" },
          { (int)Role::Enabled, "enabled" },
          { (int)Role::Ok, "ok" },
      })
{
    auto cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    m_tileDir = cacheDir + "/tiles";
    if (!QDir(m_tileDir).exists()) {
        QDir dir(cacheDir);

        if (!dir.mkpath("tiles")) {
            qWarning() << "Unable to create tile dir" << m_tileDir;
        }
    }

    QSettings settings(orgName, appName);
    setDir(settings.value(chartDirKey).toString());

    connect(&m_serverPollTimer, &QTimer::timeout, this, [&]() {
        if (m_oesencServerControl->isReady()) {
            populateModel(m_dir);
            m_serverPollTimer.stop();
        }

        m_serverPollDuration += serverPollInterval;

        if (m_serverPollDuration > serverPollTimeout) {
            m_serverError = true;
            emit serverErrorChanged();
            m_serverPollTimer.stop();
        }
    });
    m_serverPollTimer.start(serverPollInterval);
}

QHash<QString, bool> ChartModel::readVisibleCharts()
{
    QSettings settings(orgName, appName);
    settings.beginGroup("VisibleCharts");
    QHash<QString, bool> converted;

    QHash<QString, QVariant> hash = settings.value(m_dir).toHash();

    const auto keys = hash.keys();
    for (const auto &key : keys) {
        converted[key] = hash[key].toBool();
    }
    return converted;
}

void ChartModel::writeVisibleCharts()
{
    QSettings settings(orgName, appName);
    settings.beginGroup("VisibleCharts");
    QHash<QString, QVariant> hash;

    for (const auto &source : m_sourceCache) {
        hash[QString::fromStdString(source.name)] = source.enabled;
    }
    settings.setValue(m_dir, hash);
}

void ChartModel::setUrl(const QUrl &url)
{
    setDir(url.toLocalFile());
}

void ChartModel::addSource(const std::shared_ptr<OesencTileSource> &tileSource, bool enabled)
{
    TileFactory::Source source;
    source.name = m_currentChartBeeingLoaded.toStdString();
    source.enabled = enabled;
    source.tileSource = tileSource;

    for (auto it = m_sourceCache.begin(); it < m_sourceCache.end(); it++) {
        if (it->name > source.name) {
            int i = std::distance(m_sourceCache.begin(), it);
            beginInsertRows(QModelIndex(), i, i);
            m_sourceCache.insert(it, source);
            endInsertRows();
            return;
        }
    }

    int index = static_cast<int>(m_sourceCache.size());
    beginInsertRows(QModelIndex(), index, index);
    m_sourceCache.push_back(source);
    endInsertRows();
}

bool ChartModel::loadNextFromQueue()
{
    if (m_chartsToLoad.isEmpty()) {
        m_loadingCharts = false;
        m_loadingProgress = 1;
        emit loadingProgressChanged();
        m_tileFactory->setSources(m_sourceCache);
        return true;
    }

    if (m_initalQueueSize > 0) {
        m_loadingProgress = static_cast<float>(m_initalQueueSize - m_chartsToLoad.size()) / m_initalQueueSize;
        emit loadingProgressChanged();
    }

    m_loadingCharts = true;
    m_currentChartBeeingLoaded = m_chartsToLoad.dequeue();

    const QFileInfo fileInfo(m_currentChartBeeingLoaded);

    std::unique_ptr<std::istream> stream;
    std::string filePath = QDir(m_dir).filePath(m_currentChartBeeingLoaded).toStdString();

    static std::mutex sourceMutex;

    auto oesencTileCreator = std::make_shared<OesencTileSource>(m_catalog.get(),
                                                                m_currentChartBeeingLoaded.toStdString(),
                                                                m_tileDir.toStdString());

    addSource(oesencTileCreator, true);
    updateAllEnabled();
    QMetaObject::invokeMethod(this, &ChartModel::loadNextFromQueue, Qt::QueuedConnection);
    return true;
}

void ChartModel::populateModel(const QString &dir)
{
    if (!m_oesencServerControl->isReady()) {
        return;
    }

    if (m_dirBeeingLoaded == dir) {
        return;
    }

    beginResetModel();
    m_sourceCache.clear();
    endResetModel();

    m_tileFactory->clear();
    m_chartsToLoad.clear();

    if (!QDir(dir).exists()) {
        qWarning() << "No such directory" << dir;
        return;
    }

    m_catalog = std::make_unique<Catalog>(m_oesencServerControl.get(), dir.toStdString());
    emit catalogTypeChanged();
    emit catalogLoadedChanged();

    if (m_catalog->type() == Catalog::Type::Invalid) {
        return;
    }

    std::vector<std::string> chartFileNames = m_catalog->chartFileNames();
    m_chartsToLoad.resize(chartFileNames.size());

    std::transform(chartFileNames.cbegin(), chartFileNames.cend(), m_chartsToLoad.begin(), [](const std::string &path) {
        return QString::fromStdString(path);
    });

    m_loadingProgress = 0;
    emit loadingProgressChanged();
    m_initalQueueSize = m_chartsToLoad.size();

    if (loadNextFromQueue()) {
        m_dirBeeingLoaded = dir;
    }
}

void ChartModel::setDir(const QString &dir)
{
    if (m_dir == dir) {
        return;
    }

    m_dir = dir;
    emit dirChanged();

    populateModel(dir);

    QSettings settings(orgName, appName);
    settings.setValue(chartDirKey, m_dir);
}

void ChartModel::setAllChartsEnabled(bool enabled)
{
    m_tileFactory->setAllChartsEnabled(enabled);

    // Emit on every for now. Not straightforward to figure out which ones when
    // m_sourceCache is not equal to m_tileFactory->sources();
    int i = 0;
    for (auto &source : m_sourceCache) {
        source.enabled = enabled;
        emit dataChanged(createIndex(i, 0),
                         createIndex(i, 0),
                         QList<int> { static_cast<int>(Role::Enabled) });
        i++;
    }

    updateAllEnabled();
}

bool ChartModel::allEnabled() const
{
    return m_allEnabled;
}

void ChartModel::setChartEnabled(int index, bool enabled)
{
    if (m_sourceCache[index].enabled == enabled) {
        return;
    }

    m_tileFactory->setChartEnabled(m_sourceCache[index].name, enabled);
    m_sourceCache[index].enabled = enabled;
    emit dataChanged(createIndex(index, 0),
                     createIndex(index, 0),
                     QList<int> { static_cast<int>(Role::Enabled) });

    updateAllEnabled();
}

void ChartModel::updateAllEnabled()
{
    bool allEnabled = true;
    for (const auto &source : m_sourceCache) {
        if (!source.enabled) {
            allEnabled = false;
            break;
        }
    }
    if (m_allEnabled != allEnabled) {
        m_allEnabled = allEnabled;
        emit allEnabledChanged();
    }
}

void ChartModel::clear()
{
    m_tileFactory->clear();
    m_dir.clear();
}

QHash<int, QByteArray> ChartModel::roleNames() const
{
    return m_roleNames;
}

int ChartModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(m_sourceCache.size());
}

QVariant ChartModel::data(const QModelIndex &index, int role) const
{
    if (index.parent().isValid()) {
        return QVariant();
    }

    int length = static_cast<int>(m_sourceCache.size());

    if (index.row() < 0 || index.row() >= length) {
        return QVariant();
    }

    bool ok = m_sourceCache[index.row()].tileSource != nullptr;

    switch (static_cast<Role>(role)) {
    case Role::Name:
        return QString::fromStdString(m_sourceCache[index.row()].name);
        break;
    case Role::Scale:
        if (ok) {
            return m_sourceCache[index.row()].tileSource->scale();
        }
        return 0;
    case Role::Enabled:
        return m_sourceCache[index.row()].enabled;
    case Role::Ok:
        return ok;
    }

    return QVariant();
}

float ChartModel::loadingProgress() const
{
    return m_loadingProgress;
}

bool ChartModel::serverError() const
{
    return m_serverError;
}

ChartModel::CatalogType ChartModel::catalogType() const
{
    if (!m_catalog) {
        return ChartModel::CatalogType::NotLoaded;
    }

    Catalog::Type type = m_catalog->type();

    switch (type) {
    case Catalog::Type::Unencrypted:
        return CatalogType::Unencrypted;
    case Catalog::Type::Oesenc:
        return CatalogType::Oesenc;
    case Catalog::Type::Oesu:
        return CatalogType::Oesu;
    default:
        return CatalogType::Invalid;
    }
}

bool ChartModel::catalogLoaded() const
{
    return m_catalog != nullptr;
}
