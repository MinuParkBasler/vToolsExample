#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QFileDialog>
#include <QDir>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("How to implement an image processing software in embedded environment");

    // Graphics view settings to show a result image
    ui->graphicsView->setScene(&scene);
    scene.addItem(&pixmapItem);

    connect(ui->pushButton_run, &QPushButton::toggled, this, [this](bool on){
        if(on){ // Start the recipe
            vTools->start();
        }else{ // Stop the recipe
            vTools->stop();
        }
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setPylon(pylonTest *pylon)
{
    vTools = pylon;
    vTools->init();
}

// Getting images from pylonTest class
void MainWindow::getImage()
{
    vTools->drawLock();
    QImage outImage = vTools->getImage();
    ui->label_result->setText("Result:" + vTools->getValue());
    pixmapItem.setPixmap(QPixmap::fromImage(outImage));
    ui->graphicsView->fitInView(&pixmapItem, Qt::KeepAspectRatio);
}

