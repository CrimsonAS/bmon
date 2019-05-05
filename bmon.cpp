#include <QApplication>
#include <QQmlApplicationEngine>

#include "processmodel.h"
#include "sortfilterproxymodel.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QQmlApplicationEngine engine;

    qmlRegisterType<ProcessModel>("no.crimson.bmon", 1, 0, "ProcessModel");
    qmlRegisterType<SortFilterProxyModel>("no.crimson.bmon", 1, 0, "SortFilterProxyModel");
    qmlRegisterType<ProcessInfo>();

    engine.load(QUrl::fromLocalFile("main.qml"));

    return app.exec();
}
