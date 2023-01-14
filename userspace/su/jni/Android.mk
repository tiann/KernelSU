LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := su
LOCAL_SRC_FILES := su.c
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := su2
LOCAL_SRC_FILES := simple_su.c
include $(BUILD_EXECUTABLE)
