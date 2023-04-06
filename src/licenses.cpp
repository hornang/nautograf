#include <QCoreApplication>
#include <QDir>

#include "licenses.h"

Licenses::Licenses()
{
    const QString installPath = QCoreApplication::applicationDirPath() + "/../licenses";
    const QString devPath = QStringLiteral("licenses");

    if (QFile::exists(installPath)) {
        setLicenseDir(installPath);
    } else if (QFile::exists(devPath)) {
        setLicenseDir(devPath);
    } else {
        qWarning() << "Unable to find license dir";
    }
}

void Licenses::setLicenseDir(const QString &path)
{
    m_licenses.clear();
    m_names.clear();

    QStringList licenseFiles = QDir(path).entryList({ QStringLiteral("*.txt") },
                                                    QDir::Files | QDir::NoDotAndDotDot);

    for (const auto &licenseFile : licenseFiles) {
        License license { licenseFile.left(licenseFile.size() - 4), licenseFile };
        m_licenses.append(license);
        m_names.append(license.name);
    }
    m_licenseDir = path;
    emit licensesChanged();
    setIndex(0);
}

QString Licenses::currentLicense() const
{
    return m_currentLicense;
}

void Licenses::load(int index)
{
    if (index < 0 || index > m_licenses.size() - 1) {
        return;
    }

    QFile file(m_licenseDir + "/" + m_licenses.at(index).file);

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray data = file.readAll();
    file.close();
    m_currentLicense = QString::fromUtf8(data);
    emit currentLicenseChanged();
}

QStringList Licenses::names() const
{
    return m_names;
}

int Licenses::index() const
{
    return m_index;
}

void Licenses::setIndex(int newIndex)
{
    if (m_index == newIndex)
        return;

    m_index = newIndex;
    emit indexChanged();
    load(newIndex);
}
