LOCAL_PATH  := $(call my-dir)
LIBGSTREAMER_PATH := $(LOCAL_PATH)/../../libgstreamer4android
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

