LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := su
LOCAL_SRC_FILES := su.c
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := simple_su
LOCAL_SRC_FILES := simple_su.c
include $(BUILD_EXECUTABLE)
