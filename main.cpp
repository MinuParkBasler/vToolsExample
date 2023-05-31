#include "mainwindow.h"
#include "pylonTest.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);


    // pylon set-up
    pylonTest pylon;
    pylon.moveToThread(&pylon);

    // MainWindow set-up
    MainWindow w;
    QObject::connect(&pylon, &pylonTest::grabbed, &w, &MainWindow::getImage);
    w.setPylon(&pylon);
    w.show();

    return a.exec();
}
