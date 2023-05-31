#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include "pylonTest.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void setPylon(pylonTest *pylon);

public slots:
    void getImage();

private:
    Ui::MainWindow *ui;
    QGraphicsScene scene;
    QGraphicsPixmapItem pixmapItem;
    pylonTest *vTools;
};
#endif // MAINWINDOW_H
