#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

#include "chartmodel.h"
#include "maptile.h"
#include "maptilemodel.h"
#include "oesenc/chartfile.h"
#include "tilefactorywrapper.h"
#include "scene/scene.h"
#include "tilefactory/tilefactory.h"
#include "usersettings.h"

#ifndef QML_DIR
#define QML_DIR "qrc:/qml"
#endif

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName("Nautograf");
    setlocale(LC_NUMERIC, "C");

    const QString iconPath = ":/icon.ico";
    application.setWindowIcon(QIcon(iconPath));

    std::shared_ptr<TileFactory> tileManager = std::make_shared<TileFactory>();
    MapTileModel mapTileModel(tileManager);
    ChartModel chartModel(tileManager);

    tileManager->setUpdateCallback([&]() {
        mapTileModel.scheduleUpdate();
    });

    tileManager->setChartsChangedCb([&](const std::vector<GeoRect> &rects) {
        mapTileModel.chartsChanged(rects);
    });

    TileFactoryWrapper tileFactoryWrapper;
    tileFactoryWrapper.setTileDataCallback([&](GeoRect rect, double pixelsPerLongitude) -> std::vector<std::shared_ptr<Chart>> {
        return tileManager->tileData(rect, pixelsPerLongitude);
    });

    UserSettings userSettings;

    qmlRegisterType<MapTile>("org.seatronomy.nautograf", 1, 0, "MapTile");
    qmlRegisterType<Scene>("org.seatronomy.nautograf", 1, 0, "Scene");

    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "MapTileModel", &mapTileModel);
    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "TileFactory", &tileFactoryWrapper);
    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "ChartModel", &chartModel);
    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "UserSettings", &userSettings);

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

    engine.load(QStringLiteral(QML_DIR) + "/main.qml");

    int result = application.exec();
    qDebug() << "Waiting for threads to finish...";
    QThreadPool::globalInstance()->waitForDone();
    return result;
}
