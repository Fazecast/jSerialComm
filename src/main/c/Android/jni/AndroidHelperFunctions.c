/*
 * AndroidHelperFunctions.c
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

#ifdef __linux__
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <asm/termios.h>
#include <asm/ioctls.h>
#include <linux/usbdevice_fs.h>
#include <asm/byteorder.h>
#include <unistd.h>
#ifndef BOTHER
#include <termios.h>
#endif
#include "AndroidHelperFunctions.h"

void push_back(struct charTupleVector* vector, const char* firstString, const char* secondString, const char* thirdString)
{
	// Allocate memory for new string storage
	vector->length++;
	char** newMemory = (char**)realloc(vector->first, vector->length*sizeof(char*));
	if (newMemory)
		vector->first = newMemory;
	newMemory = (char**)realloc(vector->second, vector->length*sizeof(char*));
	if (newMemory)
		vector->second = newMemory;
	newMemory = (char**)realloc(vector->third, vector->length*sizeof(char*));
	if (newMemory)
		vector->third = newMemory;

	// Store new strings
	vector->first[vector->length-1] = (char*)malloc(strlen(firstString)+1);
	vector->second[vector->length-1] = (char*)malloc(strlen(secondString)+1);
	vector->third[vector->length-1] = (char*)malloc(strlen(thirdString)+1);
	strcpy(vector->first[vector->length-1], firstString);
	strcpy(vector->second[vector->length-1], secondString);
	strcpy(vector->third[vector->length-1], thirdString);
}

char keyExists(struct charTupleVector* vector, const char* key)
{
	size_t i;
	for (i = 0; i < vector->length; ++i)
		if (strcmp(key, vector->first[i]) == 0)
			return 1;
	return 0;
}

void getFriendlyName(const char* productFile, char* friendlyName)
{
	int friendlyNameLength = 0;
	friendlyName[0] = '\0';

	FILE *input = fopen(productFile, "rb");
	if (input)
	{
		char ch = getc(input);
		while ((ch != '\n') && ((int)ch != EOF))
		{
			friendlyName[friendlyNameLength++] = ch;
			ch = getc(input);
		}
		friendlyName[friendlyNameLength] = '\0';
		fclose(input);
	}
}

void getDriverName(const char* directoryToSearch, char* friendlyName)
{
	friendlyName[0] = '\0';

	// Open the directory
	DIR *directoryIterator = opendir(directoryToSearch);
	if (!directoryIterator)
		return;

	// Read all sub-directories in the current directory
	struct dirent *directoryEntry = readdir(directoryIterator);
	while (directoryEntry)
	{
		// Check if entry is a valid sub-directory
		if (directoryEntry->d_name[0] != '.')
		{
			// Get the readable part of the driver name
			strcpy(friendlyName, "USB-to-Serial Port (");
			char *startingPoint = strchr(directoryEntry->d_name, ':');
			if (startingPoint != NULL)
				strcat(friendlyName, startingPoint+1);
			else
				strcat(friendlyName, directoryEntry->d_name);
			strcat(friendlyName, ")");
			break;
		}
		directoryEntry = readdir(directoryIterator);
	}

	// Close the directory
	closedir(directoryIterator);
}

void recursiveSearchForComPorts(charTupleVector* comPorts, const char* fullPathToSearch)
{
	// Open the directory
	DIR *directoryIterator = opendir(fullPathToSearch);
	if (!directoryIterator)
		return;

	// Read all sub-directories in the current directory
	struct dirent *directoryEntry = readdir(directoryIterator);
	while (directoryEntry)
	{
		// Check if entry is a sub-directory
		if (directoryEntry->d_type == DT_DIR)
		{
			// Only process non-dot, non-virtual directories
			if ((directoryEntry->d_name[0] != '.') && (strcmp(directoryEntry->d_name, "virtual") != 0))
			{
				// See if the directory names a potential serial port
				if ((strlen(directoryEntry->d_name) > 3) && (directoryEntry->d_name[0] == 't') && (directoryEntry->d_name[1] == 't') && (directoryEntry->d_name[2] == 'y'))
				{
					// Determine system name of port
					char* systemName = (char*)malloc(256);
					strcpy(systemName, "/dev/");
					strcat(systemName, directoryEntry->d_name);

					// See if device has a registered friendly name
					char* friendlyName = (char*)malloc(256);
					char* productFile = (char*)malloc(strlen(fullPathToSearch) + strlen(directoryEntry->d_name) + 30);
					strcpy(productFile, fullPathToSearch);
					strcat(productFile, directoryEntry->d_name);
					strcat(productFile, "/device/../product");
					getFriendlyName(productFile, friendlyName);
					if (friendlyName[0] == '\0')
					{
						// Get friendly name based on the driver loaded
						strcpy(productFile, fullPathToSearch);
						strcat(productFile, directoryEntry->d_name);
						strcat(productFile, "/driver/module/drivers");
						getDriverName(productFile, friendlyName);
						if (friendlyName[0] != '\0')
							push_back(comPorts, systemName, friendlyName, friendlyName);
					}
					else
						push_back(comPorts, systemName, friendlyName, friendlyName);

					// Clean up memory
					free(productFile);
					free(systemName);
					free(friendlyName);
				}
				else
				{
					// Search for more serial ports within the directory
					charTupleVector newComPorts = { (char**)malloc(1), (char**)malloc(1), (char**)malloc(1), 0 };
					char* nextDirectory = (char*)malloc(strlen(fullPathToSearch) + strlen(directoryEntry->d_name) + 5);
					strcpy(nextDirectory, fullPathToSearch);
					strcat(nextDirectory, directoryEntry->d_name);
					strcat(nextDirectory, "/");
					recursiveSearchForComPorts(&newComPorts, nextDirectory);
					free(nextDirectory);
					int i;
					for (i = 0; i < newComPorts.length; ++i)
					{
						push_back(comPorts, newComPorts.first[i], newComPorts.second[i], newComPorts.third[i]);
						free(newComPorts.first[i]);
						free(newComPorts.second[i]);
						free(newComPorts.third[i]);
					}
					free(newComPorts.first);
					free(newComPorts.second);
					free(newComPorts.third);
				}
			}
		}
		directoryEntry = readdir(directoryIterator);
	}

	// Close the directory
	closedir(directoryIterator);
}

unsigned int getBaudRateCode(int baudRate)
{
	switch (baudRate)
	{
		case 50:
			return B50;
		case 75:
			return B75;
		case 110:
			return B110;
		case 134:
			return B134;
		case 150:
			return B150;
		case 200:
			return B200;
		case 300:
			return B300;
		case 600:
			return B600;
		case 1200:
			return B1200;
		case 1800:
			return B1800;
		case 2400:
			return B2400;
		case 4800:
			return B4800;
		case 9600:
			return B9600;
		case 19200:
			return B19200;
		case 38400:
			return B38400;
		case 57600:
			return B57600;
		case 115200:
			return B115200;
		case 230400:
			return B230400;
		case 460800:
			return B460800;
		case 500000:
			return B500000;
		case 576000:
			return B576000;
		case 921600:
			return B921600;
		default:
			return 0;
	}

	return 0;
}

void setBaudRate(int portFD, int baudRate)
{
#ifdef BOTHER
	struct termios2 options = { 0 };

	if (isatty(portFD))
		ioctl(portFD, TCGETS2, &options);
	else
		return;
//	{
//		struct usbdevfs_ioctl requestWrapper;
//		requestWrapper.ifno = 1;// TODO
//		requestWrapper.ioctl_code = TCGETS2;
//		requestWrapper.data = &options;
//		ioctl(portFD, USBDEVFS_IOCTL, &requestWrapper);
//	}
	options.c_cflag &= ~CBAUD;
	options.c_cflag |= BOTHER;
	options.c_ispeed = baudRate;
	options.c_ospeed = baudRate;
	if (isatty(portFD))
		ioctl(portFD, TCSETS2, &options);
	else
		return;
//	{
//		struct usbdevfs_ioctl requestWrapper;
//		requestWrapper.ifno = 1;// TODO
//		requestWrapper.ioctl_code = TCSETS2;
//		requestWrapper.data = &options;
//		ioctl(portFD, USBDEVFS_IOCTL, &requestWrapper);
//	}
#else
	struct termios options = { 0 };
	if (isatty(portFD))
		ioctl(portFD, TCGETS, &options);
	else
		return;
//	{
//		struct usbdevfs_ioctl requestWrapper;
//		requestWrapper.ifno = 1;// TODO
//		requestWrapper.ioctl_code = TCGETS;
//		requestWrapper.data = &options;
//		ioctl(portFD, USBDEVFS_IOCTL, &requestWrapper);
//	}
	cfsetispeed(&options, B38400);
	cfsetospeed(&options, B38400);
	if (isatty(portFD))
		ioctl(portFD, TCSETS, &options);
	else
		return;
//	{
//		struct usbdevfs_ioctl requestWrapper;
//		requestWrapper.ifno = 1;// TODO
//		requestWrapper.ioctl_code = TCSETS;
//		requestWrapper.data = &options;
//		ioctl(portFD, USBDEVFS_IOCTL, &requestWrapper);
//	}
#endif
}

#endif
