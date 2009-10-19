LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	Camera.cpp \
	CameraParameters.cpp \
	EGLDisplaySurface.cpp \
	EGLNativeWindowSurface.cpp \
	EventHub.cpp \
	EventRecurrence.cpp \
	KeyLayoutMap.cpp \
	KeyCharacterMap.cpp \
	ICamera.cpp \
	ICameraClient.cpp \
	ICameraService.cpp \
	IOverlay.cpp \
	ISurfaceComposer.cpp \
	ISurface.cpp \
	ISurfaceFlingerClient.cpp \
	LayerState.cpp \
	Overlay.cpp \
	PixelFormat.cpp \
	Rect.cpp \
	Region.cpp \
	Surface.cpp \
	SurfaceComposerClient.cpp \
	SurfaceFlingerSynchro.cpp 

LOCAL_SHARED_LIBRARIES := \
	libcorecg \
	libcutils \
	libutils \
	libpixelflinger \
	libhardware \
	libhardware_legacy \

LOCAL_MODULE:= libui

ifeq ($(strip $(BOARD_USES_MOUSE)),true)
LOCAL_CFLAGS += -DBOARD_USES_MOUSE
endif

ifeq ($(strip $(BOARD_USES_TSLIB)),true)
LOCAL_SRC_FILES += ITSLib.cpp
LOCAL_C_INCLUDES += external/tslib/src
LOCAL_SHARED_LIBRARIES += libts
LOCAL_CFLAGS += -DBOARD_USES_TSLIB
endif

include $(BUILD_SHARED_LIBRARY)
