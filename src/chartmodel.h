#pragma once

#include <chrono>
#include <memory>

#include <QAbstractListModel>
#include <QHash>
#include <QQueue>
#include <QTimer>
#include <QUrl>

#include <oesenc/keylistreader.h>
#include <oesenc/servercontrol.h>

#include "scene/tilefactorywrapper.h"
#include "tilefactory/catalog.h"
#include "tilefactory/tilefactory.h"

class OesencTileSource;

class ChartModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool allEnabled READ allEnabled NOTIFY allEnabledChanged)
    Q_PROPERTY(float loadingProgress READ loadingProgress NOTIFY loadingProgressChanged)
    Q_PROPERTY(bool serverError READ serverError NOTIFY serverErrorChanged)
    Q_PROPERTY(QString dir READ dir WRITE setDir NOTIFY dirChanged)
    Q_PROPERTY(bool catalogLoaded READ catalogLoaded NOTIFY catalogLoadedChanged)
    Q_PROPERTY(CatalogType catalogType READ catalogType NOTIFY catalogTypeChanged)

public:
    enum class CatalogType {
        NotLoaded,
        Oesu,
        Oesenc,
        Unencrypted,
        Invalid,
    };
    Q_ENUM(CatalogType)

    enum class Role : int {
        Name,
        Scale,
        Enabled,
        Ok,
    };

    ChartModel(std::shared_ptr<TileFactory> tileFactory);
    QHash<int, QByteArray> roleNames() const;
    bool allEnabled() const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QString dir() const { return m_dir; }
    void setDir(const QString &dir);

    float loadingProgress() const;

    bool serverError() const;
    QString cryptReaderStatus() const;
    CatalogType catalogType() const;

    bool catalogLoaded() const;

public slots:
    void populateModel(const QString &dir);
    void setChartEnabled(int index, bool enabled);
    void setAllChartsEnabled(bool enabled);
    void setUrl(const QUrl &url);
    void writeVisibleCharts();
    void clear();

signals:
    void loadChart(const QString &chart);
    void chartListAvailable(const QStringList &charts);
    void allEnabledChanged();
    void dirChanged();
    void loadingProgressChanged();
    void serverErrorChanged();
    void catalogTypeChanged();
    void catalogLoadedChanged();

private:
    QHash<QString, bool> readVisibleCharts();
    void addSource(const std::shared_ptr<OesencTileSource> &tileSource, bool enabled);
    void updateAllEnabled();
    bool loadNextFromQueue();
    QQueue<QString> m_chartsToLoad;
    std::unique_ptr<oesenc::ServerControl> m_oesencServerControl;
    std::shared_ptr<TileFactory> m_tileFactory;
    QHash<int, QByteArray> m_roleNames;
    std::vector<TileFactory::Source> m_sourceCache;
    QString m_tileDir;
    QString m_dirBeeingLoaded;
    QString m_currentChartBeeingLoaded;
    QString m_dir;
    QByteArray m_key;
    QTimer m_serverPollTimer;
    std::unique_ptr<Catalog> m_catalog;
    std::chrono::milliseconds m_serverPollDuration { 0 };
    int m_initalQueueSize = 0;
    float m_loadingProgress = 1;
    bool m_loadingCharts = false;
    bool m_allEnabled = false;
    bool m_serverError = false;
    bool m_catalogLoaded;
};
