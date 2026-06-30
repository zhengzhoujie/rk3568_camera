KERNEL_DIR ?= /home/zzj/linux/rk3658/img/rk3568_linux_sdk/kernel
ARCH ?= arm64
CROSS_COMPILE ?= /home/zzj/linux/rk3658/img/rk3568_linux_sdk/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-

QMAKE := /home/zzj/linux/rk3658/img/rk3568_linux_sdk/buildroot/output/rockchip_rk3568/host/bin/qmake

PWD := $(shell pwd)

DRIVER_DIR := $(PWD)/drivers
APP_DIR := $(PWD)/apps
OUT_DIR := $(PWD)/out

QT_PRO := $(APP_DIR)/qt_test.pro
QT_BUILD_DIR := $(APP_DIR)/build-qt
QT_TARGET := qt_test

JOBS ?= 16

all: drivers c_apps qt_app install

drivers:
	$(MAKE) -C $(KERNEL_DIR) M=$(DRIVER_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

c_apps:
	$(MAKE) -C $(APP_DIR) CROSS_COMPILE=$(CROSS_COMPILE)

qt_app:
	mkdir -p $(QT_BUILD_DIR)
	cd $(QT_BUILD_DIR) && $(QMAKE) $(QT_PRO)
	$(MAKE) -C $(QT_BUILD_DIR) -j$(JOBS)


clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(DRIVER_DIR) clean
	$(MAKE) -C $(APP_DIR) clean
	rm -rf $(QT_BUILD_DIR)
	rm -rf $(OUT_DIR)

.PHONY: all drivers c_apps qt_app install clean