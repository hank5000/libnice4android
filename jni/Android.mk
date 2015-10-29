LOCAL_PATH  := $(call my-dir)
LIBGSTREAMER_PATH := $(LOCAL_PATH)/../../libgstreamer4android

ENABLE_BUILD_EXECUTABLE := false
ENABLE_BUILD_SHARED := true

ENABLE_SAMPLE := simple
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
LOCAL_MODULE := gstreamer_android
LOCAL_SRC_FILES := $(LIBGSTREAMER_PATH)/libgstreamer_android.so
include $(PREBUILT_SHARED_LIBRARY)


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
LOCAL_CFLAGS            += -DHAVE_CONFIG_H

NICE_DIRS               :=  $(LOCAL_PATH)/ \
                            $(LOCAL_PATH)/agent/ \
                            $(LOCAL_PATH)/nice/ \
                            $(LOCAL_PATH)/random/ \
                            $(LOCAL_PATH)/socket/ \
                            $(LOCAL_PATH)/stun/ \
                            $(LOCAL_PATH)/stun/usages/


LIBGSTREAMER_INCLUDE    := $(LIBGSTREAMER_PATH)/include/glib-2.0/ \
                           $(LIBGSTREAMER_PATH)/include/glib-2.0/glib/ \
			               $(LIBGSTREAMER_PATH)/include/

NICE_INCLUDES           := $(NICE_DIRS)
NICE_SRC                := $(filter-out %test.c, $(foreach dir, $(NICE_DIRS), $(patsubst $(LOCAL_PATH)/%, %, $(wildcard $(addsuffix *.c, $(dir)))) ))

LOCAL_C_INCLUDES        := $(NICE_INCLUDES) $(LIBGSTREAMER_INCLUDE) #add your own headers if needed

LOCAL_SRC_FILES         := $(NICE_SRC)

ifeq ($(ENABLE_SAMPLE),sdp)
LOCAL_SRC_FILES         += $(LOCAL_PATH)/examples/sdp-example.c
endif

ifeq ($(ENABLE_SAMPLE),simple)
LOCAL_SRC_FILES         += $(LOCAL_PATH)/examples/simple-example.c
endif

ifeq ($(ENABLE_SAMPLE),threaded)
LOCAL_SRC_FILES         += $(LOCAL_PATH)/examples/threaded-example.c
endif



ifeq ($(ENABLE_BUILD_SHARED), true)
include $(BUILD_SHARED_LIBRARY)
endif


ifeq ($(ENABLE_BUILD_EXECUTABLE), true)
include $(BUILD_EXECUTABLE)
endif

