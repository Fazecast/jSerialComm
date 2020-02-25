LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := jSerialComm
TARGET_OUT := ../../resources/Android/$(TARGET_ARCH_ABI)
LOCAL_SRC_FILES := ../SerialPort_Posix.c ../PosixHelperFunctions.c
LOCAL_LDLIBS := -llog
LOCAL_CFLAGS := -fsigned-char

include $(BUILD_SHARED_LIBRARY)

all:
	rmdir /Q /S libs obj
