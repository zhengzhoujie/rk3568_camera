#ifndef CAMERATHREAD_H
#define CAMERATHREAD_H

#include <QThread>
#include <QImage>
#include <QString>
#include <atomic>

class CameraThread : public QThread
{
    Q_OBJECT

public:
    explicit CameraThread(const QString &device, int width, int height, QObject *parent = nullptr);
    ~CameraThread();

    void stop();

signals:
    void imageReady(const QImage &img);
    void errorOccurred(const QString &msg);

protected:
    void run() override;

private:
    QString deviceName;
    int frameWidth;
    int frameHeight;
    std::atomic<bool> running;
};

#endif