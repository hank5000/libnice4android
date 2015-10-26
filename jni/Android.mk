LOCAL_PATH  := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := gstreamer_android
LOCAL_SRC_FILES := ./libgstreamer_android.so
include $(PREBUILT_SHARED_LIBRARY)


include $(CLEAR_VARS)
NICE                    := libnice-0.1.12
LOCAL_MODULE            := nicejni
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
                            #$(LOCAL_PATH)/stun/tools/ \


OTHER_INCLUDE           := $(LOCAL_PATH)/include/glib-2.0/ \
                           $(LOCAL_PATH)/include/glib-2.0/glib/ \
			   $(LOCAL_PATH)/jni/

NICE_INCLUDES           := $(NICE_DIRS)
NICE_SRC                := $(filter-out %test.c, $(foreach dir, $(NICE_DIRS), $(patsubst $(LOCAL_PATH)/%, %, $(wildcard $(addsuffix *.c, $(dir)))) )) \
                           $(LOCAL_PATH)/examples/simple-example.c

LOCAL_C_INCLUDES        := $(NICE_INCLUDES) $(OTHER_INCLUDE) #add your own headers if needed

LOCAL_SRC_FILES         := [YOUR_SRC_FILES] \
                            $(NICE_SRC)

include $(BUILD_SHARED_LIBRARY)
