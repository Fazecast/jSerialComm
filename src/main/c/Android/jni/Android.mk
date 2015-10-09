LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := jSerialComm
LOCAL_SRC_FILES := SerialPort_Android.c AndroidHelperFunctions.c
LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)

all:
	cp -rf libs/* ../../resources/Android
	rm -rf libs/* libs obj
