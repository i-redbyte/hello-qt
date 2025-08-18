#include <QApplication>
#include "widgets/NetworkMonitorWidget.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    NetworkMonitorWidget w;
    w.setWindowTitle(QObject::tr("Simple Network Monitor (Qt)"));
    w.resize(700, 460);
    w.show();

    return app.exec();
}