#include <QObject>
#include <QString>
#include <QVariantMap>

#include "tilefactory/tilefactory.h"
#include "tilefactorywrapper.h"

class TileInfoBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(TileFactoryWrapper *tileFactory READ tileFactory WRITE setTileFactory NOTIFY tileFactoryChanged)
    Q_PROPERTY(QVariantMap tileRef READ tileRef WRITE setTileRef NOTIFY tileRefChanged)
    Q_PROPERTY(QString tileId READ tileId NOTIFY tileIdChanged)
    Q_PROPERTY(QVariantList chartInfos READ chartInfos NOTIFY chartInfosChanged)

public:
    TileInfoBackend();
    TileFactoryWrapper *tileFactory() const;
    void setTileFactory(TileFactoryWrapper *newTileFactory);
    const QString &tileId() const;
    void setTileId(const QString &newTileId);
    const QVariantMap &tileRef() const;
    void setTileRef(const QVariantMap &newTileRef);
    QVariantList chartInfos() const;

public slots:
    void tileDataChanged(const QStringList &tileIds);
    void setChartEnabled(const QString &chart, bool enabled);

signals:
    void tileFactoryChanged();
    void tileIdChanged();
    void tileRefChanged();
    void chartInfosChanged();

private:
    void update();
    TileFactoryWrapper *m_tileFactory = nullptr;
    QVariantMap m_tileRef;
    std::vector<TileFactory::ChartInfo> m_chartInfos;
    QString m_tileId;
};
