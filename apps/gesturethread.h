#ifndef GESTURETHREAD_H
#define GESTURETHREAD_H

#include <QThread>
#include <QImage>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <QRect>
#include <QSize>

class GestureThread : public QThread
{
    Q_OBJECT

public:
    explicit GestureThread(QObject *parent = nullptr);
    ~GestureThread();

    void submitFrame(const QImage &img, const QSize &originalSize);
    void stop();

signals:
    void gestureReady(const QString &result, const QRect &box);

protected:
    void run() override;

private:
    bool recognizeGesture(const QImage &img,
                          const QSize &originalSize,
                          QString &result,
                          QRect &box);

private:
    QMutex mutex;
    QWaitCondition condition;

    QImage latestFrame;
    QSize latestOriginalSize;

    bool hasFrame;
    bool running;
};

#endif