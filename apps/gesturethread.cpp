#include "gesturethread.h"

#include <QDebug>
#include <opencv2/opencv.hpp>
#include <cmath>
#include <vector>
GestureThread::GestureThread(QObject *parent)
    : QThread(parent),
      hasFrame(false),
      running(false)
{
}

GestureThread::~GestureThread()
{
    stop();
    wait();
}

void GestureThread::stop()
{
    mutex.lock();
    running = false;
    condition.wakeOne();
    mutex.unlock();
}

void GestureThread::submitFrame(const QImage &img, const QSize &originalSize)
{
    mutex.lock();

    latestFrame = img.copy();
    latestOriginalSize = originalSize;
    hasFrame = true;

    condition.wakeOne();
    mutex.unlock();
}

void GestureThread::run()
{
    running = true;

    while (true) {

        mutex.lock();

        while (!hasFrame && running) {
            condition.wait(&mutex);
        }

        if (!running) {
            mutex.unlock();
            break;
        }

        QImage frame = latestFrame;
        hasFrame = false;

        mutex.unlock();

        QString result;
        QRect box;
        bool ok = recognizeGesture(frame, latestOriginalSize, result, box);
        qDebug() << "识别结果:" << result
         << "box x:" << box.x()
         << "y:" << box.y()
         << "w:" << box.width()
         << "h:" << box.height();
        if (ok) {
            emit gestureReady(result, box);
        }
    }
}

static double angleBetween(const cv::Point &a,
                           const cv::Point &b,
                           const cv::Point &c)
{
    // 计算 ∠abc，也就是 b 点夹角
    double abx = a.x - b.x;
    double aby = a.y - b.y;
    double cbx = c.x - b.x;
    double cby = c.y - b.y;

    double dot = abx * cbx + aby * cby;
    double lab = std::sqrt(abx * abx + aby * aby);
    double lcb = std::sqrt(cbx * cbx + cby * cby);

    if (lab < 1e-6 || lcb < 1e-6)
        return 180.0;

    double cosv = dot / (lab * lcb);

    if (cosv < -1.0)
        cosv = -1.0;
    if (cosv > 1.0)
        cosv = 1.0;

    return std::acos(cosv) * 180.0 / 3.1415926;
}

bool GestureThread::recognizeGesture(const QImage &img,
                                     const QSize &originalSize,
                                     QString &result,
                                     QRect &box)
{
    if (img.isNull())
        return false;

    QImage rgbImage = img.convertToFormat(QImage::Format_RGB888);

    cv::Mat rgb(rgbImage.height(),
                rgbImage.width(),
                CV_8UC3,
                const_cast<uchar *>(rgbImage.bits()),
                rgbImage.bytesPerLine());

    cv::Mat ycrcb;
    cv::cvtColor(rgb, ycrcb, cv::COLOR_RGB2YCrCb);

    cv::Mat mask;

    // 简单肤色阈值：YCrCb 中 Cr/Cb 范围
    // 这个范围不是绝对准确，后面可以根据光照调
    cv::inRange(ycrcb,
                cv::Scalar(0, 133, 77),
                cv::Scalar(255, 173, 127),
                mask);

    // 去噪
    cv::erode(mask, mask, cv::Mat(), cv::Point(-1, -1), 1);
    cv::dilate(mask, mask, cv::Mat(), cv::Point(-1, -1), 2);

    cv::GaussianBlur(mask, mask, cv::Size(5, 5), 0);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask,
                     contours,
                     cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) {
        result = "未识别";
        return false;
    }

    int maxIndex = -1;
    double maxArea = 0.0;

    for (int i = 0; i < static_cast<int>(contours.size()); i++) {
        double area = cv::contourArea(contours[i]);

        if (area > maxArea) {
            maxArea = area;
            maxIndex = i;
        }
    }

    if (maxIndex < 0 || maxArea < 800.0) {
        result = "未识别";
        return false;
    }

    std::vector<cv::Point> contour = contours[maxIndex];

    cv::Rect smallRect = cv::boundingRect(contour);

    // 计算凸包索引
    std::vector<int> hullIndices;
    cv::convexHull(contour, hullIndices, false, false);

    int fingerCount = 1;

    if (hullIndices.size() > 3) {
        std::vector<cv::Vec4i> defects;
        cv::convexityDefects(contour, hullIndices, defects);

        int validDefects = 0;

        for (size_t i = 0; i < defects.size(); i++) {
            int startIdx = defects[i][0];
            int endIdx = defects[i][1];
            int farIdx = defects[i][2];

            float depth = defects[i][3] / 256.0f;

            cv::Point start = contour[startIdx];
            cv::Point end = contour[endIdx];
            cv::Point far = contour[farIdx];

            double angle = angleBetween(start, far, end);

            // 深度够大、夹角够小，认为是两个手指之间的缝
            if (depth > 10.0f && angle < 90.0) {
                validDefects++;
            }
        }

        fingerCount = validDefects + 1;

        if (fingerCount < 1)
            fingerCount = 1;
        if (fingerCount > 5)
            fingerCount = 5;
    }

    result = QString::number(fingerCount);

    // 小图坐标放大回原图坐标
    float scaleX = originalSize.width() / static_cast<float>(rgbImage.width());
    float scaleY = originalSize.height() / static_cast<float>(rgbImage.height());

    box = QRect(static_cast<int>(smallRect.x * scaleX),
                static_cast<int>(smallRect.y * scaleY),
                static_cast<int>(smallRect.width * scaleX),
                static_cast<int>(smallRect.height * scaleY));

    return true;
}