LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    clz.cpp.arm \
    DisplayHardware/DisplayHardware.cpp \
    DisplayHardware/DisplayHardwareBase.cpp \
    GPUHardware/GPUHardware.cpp \
    BlurFilter.cpp.arm \
    CPUGauge.cpp \
    Layer.cpp \
    LayerBase.cpp \
    LayerBuffer.cpp \
    LayerBlur.cpp \
    LayerBitmap.cpp \
    LayerDim.cpp \
    LayerOrientationAnim.cpp \
    OrientationAnimation.cpp \
    SurfaceFlinger.cpp \
    Tokenizer.cpp \
    Transform.cpp \
    VRamHeap.cpp


# need "-lrt" on Linux simulator to pick up clock_gettime
ifeq ($(TARGET_SIMULATOR),true)
	ifeq ($(HOST_OS),linux)
		LOCAL_LDLIBS += -lrt
	endif
endif

ifneq ($(USE_CUSTOM_SURFACEFLINGER_HEAP_MAX),)
LOCAL_CFLAGS += -DSURFACEFLINGER_HEAP_MAX=$(USE_CUSTOM_SURFACEFLINGER_HEAP_MAX)
else
LOCAL_CFLAGS += -DSURFACEFLINGER_HEAP_MAX=8
endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DSIGUNUSED=20
endif

LOCAL_SHARED_LIBRARIES := \
	libhardware \
	libutils \
	libcutils \
	libui \
	libcorecg \
	libsgl \
	libpixelflinger \
	libEGL \
	libGLESv1_CM

LOCAL_C_INCLUDES := \
	$(call include-path-for, corecg graphics)

LOCAL_MODULE:= libsurfaceflinger

include $(BUILD_SHARED_LIBRARY)
