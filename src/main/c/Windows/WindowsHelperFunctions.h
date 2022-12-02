/*
 * WindowsHelperFunctions.h
 *
 *       Created on:  May 05, 2015
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

#ifndef __WINDOWS_HELPER_FUNCTIONS_HEADER_H__
#define __WINDOWS_HELPER_FUNCTIONS_HEADER_H__

// Serial port JNI header file
#include "../com_fazecast_jSerialComm_SerialPort.h"

// Serial port data structure
typedef struct serialPort
{
	void *handle;
	char *readBuffer;
	wchar_t *portPath, *friendlyName, *portDescription, *portLocation;
	int errorLineNumber, errorNumber, readBufferLength, vendorID, productID;
	volatile char enumerated, eventListenerRunning;
	char serialNumber[16];
} serialPort;

// Common storage functionality
typedef struct serialPortVector
{
	serialPort **ports;
	int length, capacity;
} serialPortVector;
serialPort* pushBack(serialPortVector* vector, const wchar_t* key, const wchar_t* friendlyName, const wchar_t* description, const wchar_t* location, int vid, int pid);
serialPort* fetchPort(serialPortVector* vector, const wchar_t* key);
void removePort(serialPortVector* vector, serialPort* port);
void cleanUpVector(serialPortVector* vector);

// Windows-specific functionality
void reduceLatencyToMinimum(const wchar_t* portName, unsigned char requestElevatedPermissions);
int getPortPathFromSerial(wchar_t* portPath, int portPathLength, const char* serialNumber);

#endif		// #ifndef __WINDOWS_HELPER_FUNCTIONS_HEADER_H__
