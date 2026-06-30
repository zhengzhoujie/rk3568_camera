QT += core gui widgets
CONFIG += c++11
TARGET = camera_qt
CONFIG += link_pkgconfig
PKGCONFIG += opencv4

SOURCES += \
    main.cpp \
    camerathread.cpp \
    gesturethread.cpp

HEADERS += \
    camerathread.h \
    gesturethread.h