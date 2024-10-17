#KERNEL_DIR=/lib/modules/5.15.0-100-generic/build/
ARCH_x86_64 := x86_64
ARCH_arm := arm64
ARCH=x86_64
$(info ARCH is set to $(ARCH)) 
ifeq ($(ARCH),$(ARCH_x86_64))
	KERNEL_DIR=/usr/src/linux-headers-5.15.0-122-generic/
	CROSS_COMPILE=x86_64-linux-gnu-
	BUILD_COMMAND := $(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) modules
else ifeq ($(ARCH),$(ARCH_arm))
	KERNEL_DIR=/home/zxy/lubancat/sdk/kernel
	CROSS_COMPILE=aarch64-linux-gnu-
	BUILD_COMMAND := $(MAKE) EXTRA_CFLAGS=-fno-pic -C $(KERNEL_DIR) M=$(CURDIR) modules
endif
export  ARCH  CROSS_COMPILE

obj-m := usb_driver.o

# usb_test-y := usb_test.o

all:
	 $(BUILD_COMMAND) 
.PHONY:clean
clean:
	$(MAKE)  -C $(KERNEL_DIR) M=$(CURDIR) clean
