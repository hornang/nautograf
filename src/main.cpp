#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

#include "chartmodel.h"
#include "chartsymbols.h"
#include "maptile.h"
#include "maptilemodel.h"
#include "oesenc/chartfile.h"
#include "tilefactorywrapper.h"
#include "tilefactory/tilefactory.h"
#include "usersettings.h"

#ifndef QML_DIR
#define QML_DIR "qrc:/qml"
#endif

#ifndef SYMBOLS_DIR
#define SYMBOLS_DIR ":/symbols"
#endif

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName("Nautograf");

    const QString iconPath = ":/icon.ico";
    application.setWindowIcon(QIcon(iconPath));

    ChartSymbols chartSymbols(SYMBOLS_DIR);

    std::shared_ptr<TileFactory> tileManager = std::make_shared<TileFactory>();
    MapTileModel mapTileModel(tileManager);
    ChartModel chartModel(tileManager);

    tileManager->setUpdateCallback([&]() {
        mapTileModel.update();
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
    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "MapTileModel", &mapTileModel);
    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "TileFactory", &tileFactoryWrapper);
    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "ChartSymbols", &chartSymbols);
    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "ChartModel", &chartModel);
    qmlRegisterSingletonInstance("org.seatronomy.nautograf", 1, 0, "UserSettings", &userSettings);

    QQmlApplicationEngine engine;

    engine.load(QStringLiteral(QML_DIR) + "/main.qml");

    int result = application.exec();
    qDebug() << "Waiting for threads to finish...";
    QThreadPool::globalInstance()->waitForDone();
    return result;
}
