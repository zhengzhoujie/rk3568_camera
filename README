本项目基于 RK3568 平台，实现 IMX415 摄像头的 V4L2 subdev 驱动，
以及用户态 Qt 图像预览程序。  
内核侧完成 sensor 上电、I2C 寄存器配置、V4L2 subdev 注册和 media graph 接入；
用户态通过 V4L2 mmap 采集 NV12 图像，转换为 RGB888 后在 Qt 界面显示，
并使用 OpenCV 做简单手势识别。

编译: 主目录make
可能的问题:驱动可能无法正确的连接isp，因为开机匹配过了，
所以没有生成节点的话，可以把驱动编译进内核;
这个项目的链路是：IMX415 sensor 通过 MIPI CSI-2 输出 RAW 图像数据，
4 lane 差分数据线加一组时钟线进入 RK3568 的 CSI/ISP 模块。
内核侧 sensor 驱动作为 V4L2 subdev 通过异步注册接入 media framework，
匹配设备树后进入 probe。

probe 里主要完成资源初始化，比如获取 xvclk、reset/power gpio、pinctrl，
初始化默认 mbus format、media entity pad、v4l2 controls，
然后调用 v4l2_async_register_subdev_sensor_common 注册 subdev。
驱动还提供 s_stream，用于开关流；开流时写入 IMX415 的寄存器表，设置曝光、
增益、vblank 等控制项，最后让 sensor 进入 streaming；关流时写 standby 寄存器。

为了让上层和 ISP 能协商格式，
驱动实现了 enum_mbus_code、enum_frame_size、enum_frame_interval、
get_fmt、set_fmt、get_selection、g_mbus_config 这些接口，
用来声明 sensor 支持的 RAW10 格式、分辨率、帧率、裁剪范围和 MIPI lane 配置。

用户态 Qt 程序打开 /dev/video0，
通过 V4L2 标准流程采集图像：先 VIDIOC_QUERYCAP 查询能力，
再 VIDIOC_S_FMT 设置 NV12 格式和分辨率，然后 VIDIOC_REQBUFS 申请 4 个 mmap buffer，
VIDIOC_QUERYBUF 获取每个 buffer 的 offset 和长度，mmap 到用户空间，
再 VIDIOC_QBUF 入队，VIDIOC_STREAMON 开始采集。循环里用 VIDIOC_DQBUF 取出一帧，
处理完之后再 VIDIOC_QBUF 放回队列。

因为采集出来是 NV12，不能直接按 RGB 显示，所以我在用户态做了 NV12 到 RGB888 的转换。
NV12 的 Y 分量是每个像素一个亮度值，UV 是 2x2 像素共用一组色度值；
在 V4L2 multi-plane 场景下，Y 和 UV 可能分在两个 plane，
也可能在同一个 plane 里连续存放，所以代码里根据 num_planes 分别处理。
转换成 QImage 后，通过 Qt signal/slot 发送到主界面显示，
同时抽帧送给 OpenCV 做简单手势识别，用的是纯数学识别。
