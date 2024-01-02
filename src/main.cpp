#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

#ifdef USE_XDG_FILE_DIALOG
#include <xdg-file-dialog/xdg-file-dialog.h>
#endif

#include "chartmodel.h"
#include "licenses.h"
#include "maptile.h"
#include "maptilemodel.h"
#include "mercatorwrapper.h"
#include "oesenc/chartfile.h"
#include "scene/scene.h"
#include "scene/tilefactorywrapper.h"
#include "tilefactory/tilefactory.h"
#include "tileinfobackend.h"
#include "usersettings.h"
#include "version.h"

#ifndef QML_DIR
#define QML_DIR "qrc:/qml"
#endif

#if defined(USE_OEXSERVERD) && defined(Q_OS_WIN)
void addDirToPath(const QString &dir)
{
    const char *pathVariable = "PATH";
    QByteArray path = qgetenv(pathVariable);
    path += QByteArray(";") + dir.toLocal8Bit();
    qputenv(pathVariable, path);
}

void addOexserverdToPath()
{
    const auto appDataFolder = QStandardPaths::locate(QStandardPaths::HomeLocation,
                                                      "AppData",
                                                      QStandardPaths::LocateDirectory);
    if (appDataFolder.isEmpty()) {
        qWarning() << "Unable to find AppData folder";
        return;
    }

    const auto oexserverdDir = QDir(appDataFolder).filePath("Local/opencpn/plugins");
    addDirToPath(oexserverdDir);
}
#endif

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName("Nautograf");
    setlocale(LC_NUMERIC, "C");

    const QString iconPath = ":/icon.ico";
    application.setWindowIcon(QIcon(iconPath));

    std::shared_ptr<TileFactory> tileFactory = std::make_shared<TileFactory>();
    MapTileModel mapTileModel(tileFactory);

#if defined(USE_OEXSERVERD) && defined(Q_OS_WIN)
    // oexserverd.exe must be in PATH for oesenc::ServerControl to find it.
    // See ChartModel:m_oesencServerControl
    addOexserverdToPath();
#endif

    ChartModel chartModel(tileFactory);

    tileFactory->setUpdateCallback([&]() {
        mapTileModel.scheduleUpdate();
    });

    tileFactory->setChartsChangedCb([&](const std::vector<GeoRect> &rects) {
        mapTileModel.chartsChanged(rects);
    });

    TileFactoryWrapper tileFactoryWrapper;
    tileFactoryWrapper.setTileDataCallback([&](GeoRect rect, double pixelsPerLongitude) -> std::vector<std::shared_ptr<Chart>> {
        return tileFactory->tileData(rect, pixelsPerLongitude);
    });
    tileFactoryWrapper.setChartInfoCallback([&](GeoRect rect, double pixelsPerLongitude) -> std::vector<TileFactory::ChartInfo> {
        return tileFactory->chartInfo(rect, pixelsPerLongitude);
    });

    tileFactory->setTileDataChangedCallback([&](const std::vector<std::string> &tileIds) {
        tileFactoryWrapper.triggerTileDataChanged(tileIds);
    });

    tileFactoryWrapper.setTileSettingsCb([&](const std::string &tileId, TileFactory::TileSettings tileSettings) {
        tileFactory->setTileSettings(tileId, tileSettings);
    });

    UserSettings userSettings;

    qmlRegisterType<MapTile>("org.seatronomy.nautograf", 1, 0, "MapTile");
    qmlRegisterType<Scene>("org.seatronomy.nautograf", 1, 0, "Scene");
    qmlRegisterType<TileInfoBackend>("org.seatronomy.nautograf", 1, 0, "TileInfoBackend");
    qmlRegisterType<Licenses>("org.seatronomy.nautograf", 1, 0, "Licenses");

    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "MapTileModel", &mapTileModel);
    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "TileFactory", &tileFactoryWrapper);
    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "ChartModel", &chartModel);
    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "UserSettings", &userSettings);

    MercatorWrapper mercatorWrapper;
    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "Mercator", &mercatorWrapper);

    QQmlApplicationEngine engine;

    QString aboutFile = QStringLiteral(QML_DIR) + "/about.md";
    if (aboutFile.startsWith("qrc")) {
        aboutFile = aboutFile.right(aboutFile.length() - 3);
    }

    QFile file(aboutFile);
    if (file.open(QIODevice::ReadOnly)) {
        auto aboutText = file.readAll();
        engine.rootContext()->setContextProperty("AboutText", aboutText);
        file.close();
    }
    engine.rootContext()->setContextProperty("AppVersion", QStringLiteral(NAUTOGRAF_VERSION));

#ifdef USE_OEXSERVERD
    constexpr bool canReadEncryptedCatalog = true;
#else
    constexpr bool canReadEncryptedCatalog = false;
#endif
    engine.rootContext()->setContextProperty("CanReadEncryptedCatalog", canReadEncryptedCatalog);

#ifdef USE_XDG_FILE_DIALOG
    qmlRegisterType<XdgFileDialog>("org.seatronomy.nautograf", 1, 0, "XdgFileDialog");
    constexpr bool useXdgFileDialog = true;
#else
    constexpr bool useXdgFileDialog = false;
#endif
    engine.rootContext()->setContextProperty("UseXdgFileDialog", useXdgFileDialog);

    engine.load(QStringLiteral(QML_DIR) + "/main.qml");

    int result = application.exec();
    qDebug() << "Waiting for threads to finish...";
    QThreadPool::globalInstance()->waitForDone();
    return result;
}
