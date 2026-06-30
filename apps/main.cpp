#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QPixmap>
#include <QFont>
#include <QTransform>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QDebug>
#include "camerathread.h"
#include "gesturethread.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString videoDev = "/dev/video0";

    QString latestGesture = "未识别";
    QRect latestBox;
    bool hasGestureBox = false;

    QWidget window;
    window.setWindowTitle("RK3568 Camera Preview");

    QVBoxLayout *layout = new QVBoxLayout(&window);

    QLabel *title = new QLabel("RK3568 IMX415 Camera");
    title->setAlignment(Qt::AlignCenter);
    title->setFont(QFont("Arial", 24));
    title->setFixedHeight(60);

    QLabel *imageLabel = new QLabel;
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setText("Opening camera...");
    imageLabel->setFont(QFont("Arial", 24));

    layout->addWidget(title);
    layout->addWidget(imageLabel, 1);

    CameraThread camera(videoDev, 1920, 1080);
    GestureThread gesture;

    int frameCount = 0;

    // 摄像头每来一帧，隔 5 帧送一张小图给识别线程
    QObject::connect(&camera, &CameraThread::imageReady,
                     [&gesture, &frameCount](const QImage &img) {
        frameCount++;

        if (frameCount % 5 != 0)
            return;

        QImage small = img.scaled(320, 180,
                                  Qt::KeepAspectRatio,
                                  Qt::FastTransformation);
        
        gesture.submitFrame(small, img.size());
    });

    //识别线程返回结果：保存最新手势和框
    QObject::connect(&gesture, &GestureThread::gestureReady,
                     &window,
                     [&latestGesture, &latestBox, &hasGestureBox](const QString &result, const QRect &box) {
        latestGesture = result;
        latestBox = box;
        hasGestureBox = true;
    });

    // 摄像头每来一帧：画框、画文字、旋转、显示
    QObject::connect(&camera, &CameraThread::imageReady,
                     [imageLabel, &latestGesture, &latestBox, &hasGestureBox](const QImage &img) {
        QImage show = img.copy();
        QPainter painter(&show);
        painter.setPen(QPen(Qt::red, 4));
        painter.setFont(QFont("Arial", 48));

        if (hasGestureBox) {
            painter.drawRect(latestBox);
            painter.drawText(latestBox.x(), latestBox.y() - 10, latestGesture);
        }

        painter.end();

        QImage rotated = show.transformed(QTransform().rotate(-90));
        imageLabel->setPixmap(QPixmap::fromImage(rotated).scaled(
            imageLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
    });

    QObject::connect(&camera, &CameraThread::errorOccurred,
                     [imageLabel](const QString &err) {
        imageLabel->setText(err);
    });
    qDebug() << "启动了";
    gesture.start();
    camera.start();

    window.showFullScreen();

    int ret = app.exec();

    camera.stop();
    camera.wait();

    gesture.stop();
    gesture.wait();

    return ret;
}