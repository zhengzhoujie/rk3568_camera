#include "camerathread.h"
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <vector>
#include <algorithm>
using namespace std;

#define buffer_count 4

struct mplane_buffer{
    void *start[VIDEO_MAX_PLANES];
    size_t length[VIDEO_MAX_PLANES];
    int num_planes;
};

static int xioctl(int fd, unsigned long request, void *arg)
{
    int ret;
    do {
        ret = ioctl(fd, request, arg);
    } while(ret==-1 && errno == EINTR);

    return ret;
}

static unsigned char clamp_to_byte(int v)
{
    if(v < 0)
        return 0;
    if(v > 255)
        return 255;
    return static_cast<unsigned char>(v);
}

static void nv12_to_rgb888(const unsigned char *y_plane,
                           const unsigned char *uv_plane,
                           int width,
                           int height,
                           int y_stride,
                           int uv_stride,
                           QImage &out)
{
    for(int i = 0; i < height; i++)
    {
        unsigned char *rgbline = out.scanLine(i);
        for(int j = 0; j < width; j++)
        {
            int y = y_plane[i * y_stride + j];
            int u = uv_plane[(i / 2) * uv_stride + (j / 2) * 2]-128;
            int v = uv_plane[(i / 2) * uv_stride + (j / 2) * 2 + 1]-128;

            int c = y-16;
            if(c < 0) c = 0;

            int r = (298 * c + 409 * v + 128) >> 8;
            int g = (298 * c - 100 * u - 208 * v + 128) >> 8;
            int b = (298 * c + 516 * u + 128) >> 8;
            
            rgbline[j * 3 + 0] = clamp_to_byte(r);
            rgbline[j * 3 + 1] = clamp_to_byte(g);
            rgbline[j * 3 + 2] = clamp_to_byte(b);
        }
    }
}

CameraThread::CameraThread(const QString &device, int width, int height, QObject *parent)
    : QThread(parent),
      deviceName(device),
      frameWidth(width),
      frameHeight(height),
      running(false)
{
}

CameraThread::~CameraThread()
{
    stop();
    wait();
}

void CameraThread::stop()
{
    running = false;
}

void CameraThread::run()
{
    int fd = open(deviceName.toStdString().c_str(),O_RDWR);
    if(fd < 0){
        emit errorOccurred(QString("打开 %1 失败,原因 %1").arg(deviceName).arg(strerror(errno)));
        return;
    }

    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));

    if(xioctl(fd, VIDIOC_QUERYCAP, &cap)<0){
        emit errorOccurred(QString("查询设备能力失败,原因 %1").arg(strerror(errno)));
        close(fd);
        return;
    }

    if(!(cap.device_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)){
        emit errorOccurred(QString("设备不支持视频采集"));
        close(fd);
        return;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = frameWidth;
    fmt.fmt.pix_mp.height = frameHeight;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.num_planes = 1;

    if(xioctl(fd, VIDIOC_S_FMT, &fmt)<0){
        emit errorOccurred(QString("设置视频格式失败,原因 %1").arg(strerror(errno)));
        close(fd);
        return;
    }

    frameHeight = fmt.fmt.pix_mp.height;
    frameWidth = fmt.fmt.pix_mp.width;
    int num_planes = fmt.fmt.pix_mp.num_planes;
    int y_stride = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
    int uv_stride = y_stride;
    if(num_planes > 1){
        uv_stride = fmt.fmt.pix_mp.plane_fmt[1].bytesperline;
    }
    qDebug()<< "相机格式:"
            << frameWidth << "x" << frameHeight
            << "页:" << num_planes
            <<"y长度:" << y_stride
            <<"uv长度:" << uv_stride;
    
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));

    req.count = buffer_count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;

    if(xioctl(fd, VIDIOC_REQBUFS, &req)<0){
        emit errorOccurred(QString("请求缓冲区失败，原因 %1").arg(strerror(errno)));
        close(fd);
        return;
    }

    if(req.count < 2){
        emit errorOccurred(QString("请求缓冲区失败，缓冲区数量不足"));
        close(fd);
        return;
    }

    vector<mplane_buffer> buffers(req.count);

    for(unsigned int i=0;i<req.count;i++){
        struct v4l2_buffer buf;
        struct v4l2_plane planes[VIDEO_MAX_PLANES];
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.m.planes = planes;
        buf.length = VIDEO_MAX_PLANES;

        if(xioctl(fd, VIDIOC_QUERYBUF, &buf)<0){
            emit errorOccurred(QString("查询缓冲区失败，原因 %1").arg(strerror(errno)));
            close(fd);
            return;
        }
        buffers[i].num_planes = buf.length;
        for(unsigned int j=0;j<buf.length;j++){
            buffers[i].length[j] = planes[j].length;
            buffers[i].start[j] = mmap(NULL, buffers[i].length[j],
                                        PROT_READ | PROT_WRITE, MAP_SHARED,
                                        fd,planes[j].m.mem_offset);
            if(buffers[i].start[j] == MAP_FAILED){
                emit errorOccurred(QString("映射缓冲区失败，原因 %1").arg(strerror(errno)));
                close(fd);
                return;
            }
        }
    }
    for(unsigned int i=0;i<req.count;i++){
        struct v4l2_buffer buf;
        struct v4l2_plane planes[VIDEO_MAX_PLANES];
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.m.planes = planes;
        buf.length = num_planes;
        if(xioctl(fd, VIDIOC_QBUF, &buf)<0){
            emit errorOccurred(QString("入队缓冲区失败，原因 %1").arg(strerror(errno)));
            close(fd);
            return;
        }
    }
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if(xioctl(fd, VIDIOC_STREAMON, &type)<0){
        emit errorOccurred(QString("启动视频流失败，原因 %1").arg(strerror(errno)));
        close(fd);
        return;
    }

    running = true;

    while(running){
        struct v4l2_buffer buf;
        struct v4l2_plane planes[VIDEO_MAX_PLANES];
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.length = num_planes;
        buf.m.planes = planes;
        if(xioctl(fd, VIDIOC_DQBUF, &buf)<0){
            if(errno == EAGAIN)
                continue;
            emit errorOccurred(QString("出队缓冲区失败，原因 %1").arg(strerror(errno)));
            break;
        }
        unsigned char *y_plane = static_cast<unsigned char *>(buffers[buf.index].start[0]);
        unsigned char *uv_plane = nullptr;
        if (num_planes > 1) {
            uv_plane = static_cast<unsigned char *>(buffers[buf.index].start[1]);
        }
        else {
            uv_plane = y_plane + y_stride * frameHeight;
        }
        QImage img(frameWidth, frameHeight, QImage::Format_RGB888);

        nv12_to_rgb888(y_plane, uv_plane, frameWidth, frameHeight,y_stride,uv_stride,img);
        
        emit imageReady(img);

        if(xioctl(fd, VIDIOC_QBUF, &buf)<0){
            emit errorOccurred(QString("入队缓冲区失败，原因 %1").arg(strerror(errno)));
            break;
        }
    }
    xioctl(fd, VIDIOC_STREAMOFF, &type);
    for(size_t i=0;i<buffers.size();i++){
        for(int j=0;j<buffers[i].num_planes;j++){
            if(buffers[i].start[j]&&buffers[i].start[j]!=MAP_FAILED){
                munmap(buffers[i].start[j], buffers[i].length[j]);
            }
        }
    }
    close(fd);
        
}
