#include <QDir>
#include <QSettings>
#include <QStandardPaths>

#include "chartmodel.h"
#include "cryptreader.h"
#include "oesenc/chartfile.h"
#include "tilefactory/oesenctilesource.h"
#include "tilefactory/tilefactory.h"
#include "usersettings.h"

namespace {
const QString chartDirKey = "ChartDir";
}

ChartModel::ChartModel(std::shared_ptr<TileFactory> tileFactory)
    : m_tileFactory(tileFactory)
    , m_roleNames({
          { (int)Role::Name, "name" },
          { (int)Role::Scale, "scale" },
          { (int)Role::Enabled, "enabled" },
          { (int)Role::Ok, "ok" },
      })
{
    auto cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    const QString tileDir = cacheDir + "/tiles";
    if (!QDir(tileDir).exists()) {
        QDir dir(cacheDir);

        if (!dir.mkpath("tiles")) {
            qWarning() << "Unable to create tile dir" << tileDir;
        }
    }
    m_tileDir = tileDir;

    connect(&m_cryptReader, &CryptReader::readyChanged, this, [&](bool ready) {
        if (ready) {
            populateModel(m_dir);
        }
    });

    connect(&m_cryptReader, &CryptReader::fileDecrypted, this, &ChartModel::chartDecrypted);
    connect(&m_cryptReader, &CryptReader::error, this, [&]() {
        qWarning() << "Failed to start oeserverd";
        m_cryptReaderError = true;
        emit cryptReaderErrorChanged();
        populateModel(m_dir);
    });

    connect(&m_cryptReader, &CryptReader::statusStringChanged, this, [&](const QString status) {
        m_cryptReaderStatus = status;
        emit cryptReaderStatusChanged();
    });

    QSettings settings(orgName, appName);
    setDir(settings.value(chartDirKey).toString());

    m_cryptReader.start();
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

void ChartModel::readUnencrypted(const QString &fileName)
{
    auto visibleCharts = readVisibleCharts();
    bool enabled = visibleCharts.value(m_currentChartBeeingLoaded, true);

    QString fullPath = QDir(m_dir).filePath(fileName);
    auto oesencTileCreator = std::make_shared<OesencTileSource>(fullPath.toStdString(),
                                                                m_currentChartBeeingLoaded.toStdString(),
                                                                m_tileDir.toStdString());
    if (oesencTileCreator->isValid()) {
        addSource(oesencTileCreator, enabled, false);
    } else {
        addSource(nullptr, enabled, false);
    }
    updateAllEnabled();
}

void ChartModel::chartDecrypted(const QByteArray &data)
{
    if (data.isEmpty()) {
        qWarning() << "Zero data from crypt file";
        loadNextFromQueue();
        return;
    }

    if (!m_loadingCharts) {
        // When this happens it means a new directory was set while still
        // loading the previous. Kind of a hack.
        loadNextFromQueue();
        return;
    }

    auto visibleCharts = readVisibleCharts();

    const char *d = data.data();
    std::vector<std::byte> dataVec;
    dataVec.resize(dataVec.size() + data.size());
    memcpy(&dataVec[dataVec.size() - data.size()], &d[0], data.size() * sizeof(std::byte));
    auto tileSource = std::make_shared<OesencTileSource>(dataVec,
                                                         m_currentChartBeeingLoaded.toStdString(),
                                                         m_tileDir.toStdString());

    bool enabled = visibleCharts.value(m_currentChartBeeingLoaded, true);

    if (tileSource->isValid()) {
        addSource(tileSource, enabled, true);
        updateAllEnabled();
    } else {
        readUnencrypted(m_currentChartBeeingLoaded);
    }
    loadNextFromQueue();
}

void ChartModel::addSource(const std::shared_ptr<OesencTileSource> &tileSource, bool enabled, bool encrypted)
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

    QString key;
    CryptReader::ChartType type = CryptReader::ChartType::Unknown;

    const QFileInfo fileInfo(m_currentChartBeeingLoaded);
    if (fileInfo.suffix() == "oesu") {
        key = QString::fromStdString(m_keyListReader.key(fileInfo.baseName().toStdString()));
        type = CryptReader::ChartType::Oesu;
    } else if (fileInfo.suffix() == "oesenc") {
        key = readUserKey(QDir(m_dir).filePath("Chartinfo.txt"));
        type = CryptReader::ChartType::Oesenc;
    }

    const QString chartFile = QDir(m_dir).filePath(m_currentChartBeeingLoaded);

    if (m_cryptReaderError) {
        readUnencrypted(chartFile);
        // Let the event loop update UI
        QMetaObject::invokeMethod(this, &ChartModel::loadNextFromQueue, Qt::QueuedConnection);
        return true;
    }

    return m_cryptReader.read(QDir(m_dir).filePath(m_currentChartBeeingLoaded), key, type);
}

void ChartModel::populateModel(const QString &dir)
{
    if (!m_cryptReader.ready() && !m_cryptReaderError) {
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

    m_chartsToLoad.append(QDir(dir).entryList({ "*.oesenc", "*.oesu" },
                                              QDir::NoFilter,
                                              QDir::Size | QDir::Reversed));

    m_loadingProgress = 0;
    emit loadingProgressChanged();
    m_initalQueueSize = m_chartsToLoad.size();

    if (loadNextFromQueue()) {
        m_dirBeeingLoaded = dir;
    }
}

QByteArray ChartModel::readUserKey(const QString &fileName)
{
    QFile file(fileName);

    if (!file.exists()) {
        qInfo() << "No info file";
        return {};
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QByteArray line = file.readLine();
    if (!line.startsWith("UserKey:")) {
        qWarning() << "Invalid format";
        return {};
    }

    return line.right(line.length() - 8);
}

void ChartModel::setDir(const QString &dir)
{
    if (m_dir == dir) {
        return;
    }

    m_dir = dir;
    emit dirChanged();

    m_key = readUserKey(QDir(dir).filePath("Chartinfo.txt"));

    for (const auto &xmlFile : QDir(dir).entryList({ "*.xml" }, QDir::Files)) {
        if (m_keyListReader.load(QDir(dir).filePath(xmlFile).toStdString()) != oesenc::KeyListReader::Status::Failed) {
            break;
        }
    }

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

bool ChartModel::cryptReaderError() const
{
    return m_cryptReaderError;
}

QString ChartModel::cryptReaderStatus() const
{
    return m_cryptReaderStatus;
}
