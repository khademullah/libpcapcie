#include "MainWindow.h"

#include <QApplication>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    MainWindow window;

    if (argc > 1) {
        window.loadTraceFile(QString::fromLocal8Bit(argv[1]));
    }

    window.show();
    return app.exec();
}
