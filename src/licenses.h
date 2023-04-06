#pragma once

#include <QObject>

class Licenses : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList names MEMBER m_names NOTIFY licensesChanged)
    Q_PROPERTY(int index READ index WRITE setIndex NOTIFY indexChanged)
    Q_PROPERTY(QString currentLicense READ currentLicense NOTIFY currentLicenseChanged)

public:
    Licenses();
    QStringList names() const;

    int index() const;
    QString currentLicense() const;
    void setIndex(int newIndex);
    void load(int index);

public slots:

signals:
    void indexChanged();
    void currentLicenseChanged();
    void licensesChanged();

private:
    QString m_licenseDir;
    void setLicenseDir(const QString &path);

    struct License
    {
        QString name;
        QString file;
    };

    QStringList m_names;
    QString m_currentLicense;
    QList<License> m_licenses;
    int m_index = -1;
};
