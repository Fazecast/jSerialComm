/*
 * PosixHelperFunctions.h
 *
 *       Created on:  Mar 10, 2015
 *  Last Updated on:  Dec 01, 2022
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2022 Fazecast, Inc.
 *
 * This file is part of jSerialComm.
 *
 * jSerialComm is free software: you can redistribute it and/or modify
 * it under the terms of either the Apache Software License, version 2, or
 * the GNU Lesser General Public License as published by the Free Software
 * Foundation, version 3 or above.
 *
 * jSerialComm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of both the GNU Lesser General Public
 * License and the Apache Software License along with jSerialComm. If not,
 * see <http://www.gnu.org/licenses/> and <http://www.apache.org/licenses/>.
 */

#ifndef __POSIX_HELPER_FUNCTIONS_HEADER_H__
#define __POSIX_HELPER_FUNCTIONS_HEADER_H__

// Serial port JNI header file
#include <pthread.h>
#include "com_fazecast_jSerialComm_SerialPort.h"

// Serial port data structure
typedef struct serialPort
{
	pthread_mutex_t eventMutex;
	pthread_cond_t eventReceived;
	pthread_t eventsThread1, eventsThread2;
	char *portPath, *friendlyName, *portDescription, *portLocation, *readBuffer;
	int errorLineNumber, errorNumber, handle, readBufferLength, eventsMask, event, vendorID, productID;
	volatile char enumerated, eventListenerRunning, eventListenerUsesThreads;
} serialPort;

// Common port storage functionality
typedef struct serialPortVector
{
	serialPort **ports;
	int length, capacity;
} serialPortVector;
serialPort* pushBack(serialPortVector* vector, const char* key, const char* friendlyName, const char* description, const char* location, int vid, int pid);
serialPort* fetchPort(serialPortVector* vector, const char* key);
void removePort(serialPortVector* vector, serialPort* port);
void cleanUpVector(serialPortVector* vector);

// Forced definitions
#ifndef CMSPAR
#define CMSPAR 010000000000
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

// Linux-specific functionality
#if defined(__linux__)

typedef int baud_rate;

#ifdef __ANDROID__

extern int ioctl(int __fd, int __request, ...);
#else
extern int ioctl(int __fd, unsigned long int __request, ...);
#endif


// Solaris-specific functionality
#elif defined(__sun__)

#define faccessat(dirfd, pathname, mode, flags) access(pathname, mode)

#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8
typedef int baud_rate;

extern int ioctl(int __fd, int __request, ...);
int flock(int fd, int op);


// FreeBSD-specific functionality
#elif defined(__FreeBSD__) || defined(__OpenBSD__)

typedef int baud_rate;


// Apple-specific functionality
#elif defined(__APPLE__)

#define fdatasync fsync

#include <termios.h>
typedef speed_t baud_rate;

#endif


// Common Posix functionality
void searchForComPorts(serialPortVector* comPorts);
baud_rate getBaudRateCode(baud_rate baudRate);
int setBaudRateCustom(int portFD, baud_rate baudRate);
int verifyAndSetUserPortGroup(const char *portFile);

#endif		// #ifndef __POSIX_HELPER_FUNCTIONS_HEADER_H__
