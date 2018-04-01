/*
 * AndroidHelperFunctions.h
 *
 *       Created on:  Mar 10, 2015
 *  Last Updated on:  Mar 25, 2016
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2018 Fazecast, Inc.
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

#ifndef __ANDROID_HELPER_FUNCTIONS_HEADER_H__
#define __ANDROID_HELPER_FUNCTIONS_HEADER_H__

typedef struct charTupleVector
{
	char **first, **second, **third;
	size_t length;
} charTupleVector;
void push_back(struct charTupleVector* vector, const char* firstString, const char* secondString, const char* thirdString);
char keyExists(struct charTupleVector* vector, const char* key);

void getDriverName(const char* directoryToSearch, char* friendlyName);
void recursiveSearchForComPorts(charTupleVector* comPorts, const char* fullPathToSearch);
void getFriendlyName(const char* productFile, char* friendlyName);
unsigned int getBaudRateCode(int baudRate);
void setBaudRate(int portFD, int baudRate);

extern int ioctl(int __fd, int __request, ...);

#endif		// #ifndef __ANDROID_HELPER_FUNCTIONS_HEADER_H__
