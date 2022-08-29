#pragma once

#include <memory>

#include <QHash>
#include <QQueue>
#include <QUrl>

#include <oesenc/keylistreader.h>

#include "cryptreader.h"
#include "tilefactorywrapper.h"
#include "tilefactory/tilefactory.h"
#include <QAbstractListModel>

class OesencTileSource;

class ChartModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool allEnabled READ allEnabled NOTIFY allEnabledChanged)
    Q_PROPERTY(float loadingProgress READ loadingProgress NOTIFY loadingProgressChanged)
    Q_PROPERTY(bool cryptReaderError READ cryptReaderError NOTIFY cryptReaderErrorChanged)
    Q_PROPERTY(QString cryptReaderStatus READ cryptReaderStatus NOTIFY cryptReaderStatusChanged)
    Q_PROPERTY(QString dir READ dir WRITE setDir NOTIFY dirChanged)

public:
    struct SourceWrapper
    {
        TileFactory::Source source;
        bool encrypted = false;
    };

    enum class Role : int {
        Name,
        Scale,
        Enabled,
        Ok,
        Encrypted,
    };

    ChartModel(std::shared_ptr<TileFactory> tileManager);
    QHash<int, QByteArray> roleNames() const;
    bool allEnabled() const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QString dir() const { return m_dir; }
    void setDir(const QString &dir);

    float loadingProgress() const;

    bool cryptReaderError() const;
    QString cryptReaderStatus() const;

public slots:
    void populateModel(const QString &dir);
    void setChartEnabled(int index, bool enabled);
    void setAllChartsEnabled(bool enabled);
    void chartDecrypted(const QByteArray &data);
    void setUrl(const QUrl &url);
    void writeVisibleCharts();
    void clear();

signals:
    void loadChart(const QString &chart);
    void chartListAvailable(const QStringList &charts);
    void allEnabledChanged();
    void dirChanged();
    void loadingProgressChanged();
    void cryptReaderErrorChanged();
    void cryptReaderStatusChanged();

private:
    void readUnencrypted(const QString &fileName);
    static QByteArray readUserKey(const QString &infoFile);
    QHash<QString, bool> readVisibleCharts();
    void addSource(const std::shared_ptr<OesencTileSource> &tileSource, bool enabled, bool encrypted);
    void updateAllEnabled();
    bool loadNextFromQueue();
    QQueue<QString> m_chartsToLoad;
    CryptReader m_cryptReader;
    std::shared_ptr<TileFactory> m_tileManager;
    QHash<int, QByteArray> m_roleNames;
    std::vector<SourceWrapper> m_sourceCache;
    std::unique_ptr<TileFactoryWrapper> m_tileFactory;
    QString m_tileDir;
    QString m_dirBeeingLoaded;
    QString m_currentChartBeeingLoaded;
    QString m_dir;
    QByteArray m_key;
    QString m_cryptReaderStatus;
    oesenc::KeyListReader m_keyListReader;
    int m_initalQueueSize = 0;
    float m_loadingProgress = 1;
    bool m_loadingCharts = false;
    bool m_allEnabled = false;
    bool m_cryptReaderError = false;
};
