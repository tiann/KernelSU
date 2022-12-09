LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := su
LOCAL_SRC_FILES := su.c
LOCAL_LDFLAGS := -static

include $(BUILD_EXECUTABLE)
