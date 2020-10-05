/*
 * PosixHelperFunctions.h
 *
 *       Created on:  Mar 10, 2015
 *  Last Updated on:  Apr 01, 2020
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2020 Fazecast, Inc.
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

// Common functionality
typedef struct charTupleVector
{
	char **first, **second, **third;
	size_t length;
} charTupleVector;
void push_back(struct charTupleVector* vector, const char* firstString, const char* secondString, const char* thirdString);
char keyExists(struct charTupleVector* vector, const char* key);

// Forced definitions
#ifndef CMSPAR
#define CMSPAR 010000000000
#endif

// Linux-specific functionality
#if defined(__linux__)
typedef int baud_rate;
#ifdef __ANDROID__
extern int ioctl(int __fd, int __request, ...);
#else
extern int ioctl(int __fd, unsigned long int __request, ...);
#endif
void getDriverName(const char* directoryToSearch, char* friendlyName);
void getFriendlyName(const char* productFile, char* friendlyName);
void getInterfaceDescription(const char* interfaceFile, char* interfaceDescription);
void recursiveSearchForComPorts(charTupleVector* comPorts, const char* fullPathToSearch);
void lastDitchSearchForComPorts(charTupleVector* comPorts);
void driverBasedSearchForComPorts(charTupleVector* comPorts);

// Solaris-specific functionality
#elif defined(__sun__)
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8
typedef int baud_rate;
extern int ioctl(int __fd, int __request, ...);
int flock(int fd, int op);
void searchForComPorts(charTupleVector* comPorts);

// Apple-specific functionality
#elif defined(__APPLE__)
#define fdatasync(a) fsync(a)
#include <termios.h>
typedef speed_t baud_rate;

#endif

// Common baud rate functionality
baud_rate getBaudRateCode(baud_rate baudRate);
int setBaudRateCustom(int portFD, baud_rate baudRate);

#endif		// #ifndef __POSIX_HELPER_FUNCTIONS_HEADER_H__
