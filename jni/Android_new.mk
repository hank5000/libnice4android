LOCAL_PATH  := $(call my-dir)



LIBGSTREAMER_ROOT_PATH :=/Users/HankWu/Downloads/gstreamer-ndk

ifeq ($(TARGET_ARCH_ABI),armeabi)
LIBGSTREAMER_PATH        := $(LIBGSTREAMER_ROOT_PATH)/arm
else ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LIBGSTREAMER_PATH        := $(LIBGSTREAMER_ROOT_PATH)/armv7
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
LIBGSTREAMER_PATH        := $(LIBGSTREAMER_ROOT_PATH)/arm64
else ifeq ($(TARGET_ARCH_ABI),x86)
LIBGSTREAMER_PATH        := $(LIBGSTREAMER_ROOT_PATH)/x86
else ifeq ($(TARGET_ARCH_ABI),x86_64)
LIBGSTREAMER_PATH        := $(LIBGSTREAMER_ROOT_PATH)/x86_64
else
$(error Target arch ABI not supported)
endif

LIBNICE_PATH      := $(LOCAL_PATH)/libnice

ENABLE_BUILD_EXECUTABLE := false
ENABLE_BUILD_SHARED := true
ENABLE_BUILD_JNI_PART := true

ENABLE_SAMPLE := sdp
# simple, threaded, sdp


# if both true, then turn executable off
ifeq ($(ENABLE_BUILD_SHARED),true)
ifeq ($(ENABLE_BUILD_EXECUTABLE),true)
ENABLE_BUILD_EXECUTABLE := false
ENABLE_BUILD_SHARED := true
endif
endif

ifeq ($(ENABLE_BUILD_EXECUTABLE),false)
ENABLE_SAMPLE :=
endif

include $(CLEAR_VARS)
NICE                    := libnice-0.1.13

ifeq ($(ENABLE_BUILD_EXECUTABLE),true)
LOCAL_MODULE            := $(ENABLE_SAMPLE)_sample
endif

ifeq ($(ENABLE_BUILD_SHARED),true)
LOCAL_MODULE            := libnice4android
endif


LOCAL_LDLIBS            := -llog 


LOCAL_SHARED_LIBRARIES  := gstreamer_android
LOCAL_CFLAGS            += -DHAVE_CONFIG_H -std=c99

NICE_DIRS               :=  $(LIBNICE_PATH)/ \
                            $(LIBNICE_PATH)/agent/ \
                            $(LIBNICE_PATH)/nice/ \
                            $(LIBNICE_PATH)/random/ \
                            $(LIBNICE_PATH)/socket/ \
                            $(LIBNICE_PATH)/stun/ \
                            $(LIBNICE_PATH)/stun/usages/


LIBGSTREAMER_INCLUDE    := $(LIBGSTREAMER_PATH)/include/glib-2.0/ \
                           $(LIBGSTREAMER_PATH)/include/glib-2.0/glib/ \
			               $(LIBGSTREAMER_PATH)/include/




NICE_INCLUDES           := $(NICE_DIRS)
NICE_SRC                := $(filter-out %test.c, $(foreach dir, $(NICE_DIRS), $(patsubst $(LOCAL_PATH)/%, %, $(wildcard $(addsuffix *.c, $(dir)))) ))


LOCAL_C_INCLUDES        := $(NICE_INCLUDES) $(LIBGSTREAMER_INCLUDE) #add your own headers if needed

LOCAL_SRC_FILES         := $(NICE_SRC)

ifeq ($(ENABLE_SAMPLE),sdp)
LOCAL_SRC_FILES         += $(LIBNICE_PATH)/examples/sdp-example.c
else ifeq ($(ENABLE_SAMPLE),simple)
LOCAL_SRC_FILES         += $(LIBNICE_PATH)/examples/simple-example.c
else ifeq ($(ENABLE_SAMPLE),threaded)
LOCAL_SRC_FILES         += $(LIBNICE_PATH)/examples/threaded-example.c
endif

ifeq ($(ENABLE_BUILD_JNI_PART),true)
LOCAL_SRC_FILES         += $(LOCAL_PATH)/libnice_jni.c
endif


ifeq ($(ENABLE_BUILD_SHARED), true)
include $(BUILD_SHARED_LIBRARY)
endif


ifeq ($(ENABLE_BUILD_EXECUTABLE), true)
include $(BUILD_EXECUTABLE)
endif


ifeq ($(TARGET_ARCH_ABI),armeabi)
GSTREAMER_ROOT        := $(LIBGSTREAMER_ROOT_PATH)/arm
else ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
GSTREAMER_ROOT        := $(LIBGSTREAMER_ROOT_PATH)/armv7
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
GSTREAMER_ROOT        := $(LIBGSTREAMER_ROOT_PATH)/arm64
else ifeq ($(TARGET_ARCH_ABI),x86)
GSTREAMER_ROOT        := $(LIBGSTREAMER_ROOT_PATH)/x86
else ifeq ($(TARGET_ARCH_ABI),x86_64)
GSTREAMER_ROOT        := $(LIBGSTREAMER_ROOT_PATH)/x86_64
else
$(error Target arch ABI not supported)
endif

ifndef GSTREAMER_ROOT
ifndef GSTREAMER_ROOT_ANDROID
$(error GSTREAMER_ROOT_ANDROID is not defined!)
endif
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)
endif

GSTREAMER_NDK_BUILD_PATH  := $(GSTREAMER_ROOT)/share/gst-android/ndk-build/

include $(GSTREAMER_NDK_BUILD_PATH)/plugins.mk
GSTREAMER_PLUGINS         := $(GSTREAMER_PLUGINS_CORE) $(GSTREAMER_PLUGINS_PLAYBACK) $(GSTREAMER_PLUGINS_CODECS) $(GSTREAMER_PLUGINS_NET) $(GSTREAMER_PLUGINS_SYS) $(GSTREAMER_PLUGINS_CODECS_RESTRICTED) $(GSTREAMER_CODECS_GPL) $(GSTREAMER_PLUGINS_ENCODING) $(GSTREAMER_PLUGINS_VIS) $(GSTREAMER_PLUGINS_EFFECTS) $(GSTREAMER_PLUGINS_NET_RESTRICTED)
GSTREAMER_EXTRA_DEPS      := gstreamer-player-1.0 gstreamer-video-1.0 glib-2.0

include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer-1.0.mk

