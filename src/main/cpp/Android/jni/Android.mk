LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := jSerialComm
LOCAL_SRC_FILES := SerialPort_Android.cpp AndroidHelperFunctions.cpp

include $(BUILD_SHARED_LIBRARY)

all:
	cp -rf libs/* ../../resources/Android/
	rm -rf libs obj/* obj
