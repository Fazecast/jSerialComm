/*
 * LinuxHelperFunctions.h
 *
 *       Created on:  Mar 10, 2015
 *  Last Updated on:  Mar 25, 2016
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2017 Fazecast, Inc.
 *
 * This file is part of jSerialComm.
 *
 * jSerialComm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * jSerialComm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with jSerialComm.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LINUX_HELPER_FUNCTIONS_HEADER_H__
#define __LINUX_HELPER_FUNCTIONS_HEADER_H__

typedef struct charPairVector
{
	char **first, **second;
	size_t length;
} charPairVector;
void push_back(struct charPairVector* vector, const char* firstString, const char* secondString);
char keyExists(struct charPairVector* vector, const char* key);

void getDriverName(const char* directoryToSearch, char* friendlyName);
void recursiveSearchForComPorts(charPairVector* comPorts, const char* fullPathToSearch);
void lastDitchSearchForComPorts(charPairVector* comPorts);
void getFriendlyName(const char* productFile, char* friendlyName);
unsigned int getBaudRateCode(int baudRate);
void setBaudRate(int portFD, int baudRate);

#endif		// #ifndef __LINUX_HELPER_FUNCTIONS_HEADER_H__
