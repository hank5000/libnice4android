LOCAL_PATH  := $(call my-dir)


ENABLE_BUILD_EXECUTABLE := false
ENABLE_BUILD_SHARED := true
ENABLE_BUILD_JNI_PART := true
COMBINE_JNI_AND_LIB := true

ENABLE_SAMPLE := sdp




LIBGSTREAMER_ROOT_PATH :=../../gstreamer-1.0-android-universal-1.9.90

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

# glib
include $(CLEAR_VARS)
LOCAL_MODULE    := glib
LOCAL_SRC_FILES := $(LIBGSTREAMER_PATH)/lib/libglib-2.0.a
LOCAL_STATIC_LIBRARIES := intl iconv

LOCAL_EXPORT_CFLAGS := 				\
	-I$(LIBGSTREAMER_PATH)/include/glib-2.0	\
	-I$(LIBGSTREAMER_PATH)/lib/glib-2.0/include	\
	$(NULL)
include $(PREBUILT_STATIC_LIBRARY)

# gobject
include $(CLEAR_VARS)
LOCAL_MODULE    := gobject
LOCAL_SRC_FILES := $(LIBGSTREAMER_PATH)/lib/libgobject-2.0.a
LOCAL_STATIC_LIBRARIES := glib
include $(PREBUILT_STATIC_LIBRARY)

# gmodule
include $(CLEAR_VARS)
LOCAL_MODULE    := gmodule
LOCAL_SRC_FILES := $(LIBGSTREAMER_PATH)/lib/libgmodule-2.0.a
LOCAL_STATIC_LIBRARIES := glib
include $(PREBUILT_STATIC_LIBRARY)

# gthread
include $(CLEAR_VARS)
LOCAL_MODULE    := gthread
LOCAL_SRC_FILES := $(LIBGSTREAMER_PATH)/lib/libgthread-2.0.a
LOCAL_STATIC_LIBRARIES := glib
include $(PREBUILT_STATIC_LIBRARY)

# gio
include $(CLEAR_VARS)
LOCAL_MODULE := gio
LOCAL_SRC_FILES := $(LIBGSTREAMER_PATH)/lib/libgio-2.0.a
LOCAL_STATIC_LIBRARIES := glib
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE    := iconv
LOCAL_SRC_FILES := $(LIBGSTREAMER_PATH)/lib/libiconv.a
LOCAL_EXPORT_C_INCLUDES := $(LIBGSTREAMER_PATH)/include/
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := intl
LOCAL_SRC_FILES := $(LIBGSTREAMER_PATH)/lib/libintl.a
LOCAL_EXPORT_C_INCLUDES := $(LIBGSTREAMER_PATH)/include/
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE    := z
LOCAL_SRC_FILES := $(LIBGSTREAMER_PATH)/lib/libz.a
LOCAL_EXPORT_C_INCLUDES := $(LIBGSTREAMER_PATH)/include/
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE    := ffi
LOCAL_SRC_FILES := $(LIBGSTREAMER_PATH)/lib/libffi.a
LOCAL_EXPORT_C_INCLUDES := $(LIBGSTREAMER_PATH)/include/
LOCAL_STATIC_LIBRARIES := glib
include $(PREBUILT_STATIC_LIBRARY)


LIBNICE_PATH      := $(LOCAL_PATH)/libnice

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


LOCAL_STATIC_LIBRARIES  := glib iconv intl gio gthread gmodule gobject z ffi
LOCAL_CFLAGS            += -DHAVE_CONFIG_H -O3 -DNDEBUG

NICE_DIRS               :=  $(LIBNICE_PATH)/ \
                            $(LIBNICE_PATH)/agent/ \
                            $(LIBNICE_PATH)/nice/ \
                            $(LIBNICE_PATH)/random/ \
                            $(LIBNICE_PATH)/socket/ \
                            $(LIBNICE_PATH)/stun/ \
                            $(LIBNICE_PATH)/stun/usages/


LIBGSTREAMER_INCLUDE    := $(LIBGSTREAMER_PATH)/include/glib-2.0/ \
                           $(LIBGSTREAMER_PATH)/include/glib-2.0/glib/ \
			               $(LIBGSTREAMER_PATH)/include/ \
                           $(LIBGSTREAMER_PATH)/lib/glib-2.0/include/


LOCAL_LDLIBS            := -llog

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

ifeq ($(COMBINE_JNI_AND_LIB),true)
ifeq ($(ENABLE_BUILD_JNI_PART),true)
LOCAL_SRC_FILES         += $(LOCAL_PATH)/libnice_jni.c
endif
endif


ifeq ($(ENABLE_BUILD_SHARED), true)
include $(BUILD_SHARED_LIBRARY)
endif


ifeq ($(ENABLE_BUILD_EXECUTABLE), true)
include $(BUILD_EXECUTABLE)
endif


ifeq ($(COMBINE_JNI_AND_LIB),false)
ifeq ($(ENABLE_BUILD_JNI_PART), true)


include $(CLEAR_VARS)
LOCAL_SRC_FILES:=$(LOCAL_PATH)/libnice_jni.c
LOCAL_SHARED_LIBRARIES:=nice4android
LOCAL_C_INCLUDES        := $(NICE_INCLUDES) $(LIBGSTREAMER_INCLUDE) #add your own headers if needed
LOCAL_CFLAGS            += -DHAVE_CONFIG_H -std=c99 -O3 -DNDEBUG
LOCAL_LDLIBS            := -llog
LOCAL_MODULE := nice4android_jni
include $(BUILD_SHARED_LIBRARY)

endif
endif
