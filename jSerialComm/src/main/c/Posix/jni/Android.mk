LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := jSerialComm
TARGET_OUT := ../../resources/Android/$(TARGET_ARCH_ABI)
LOCAL_SRC_FILES := ../SerialPort_Posix.c ../PosixHelperFunctions.c
LOCAL_LDLIBS := -llog
LOCAL_CFLAGS := -fsigned-char

include $(BUILD_SHARED_LIBRARY)

HOST_OS := $(strip $(HOST_OS))
ifeq ($(OS),Windows_NT)
all:
	rmdir /Q /S libs obj
else
all:
	rm -rf libs obj
endif