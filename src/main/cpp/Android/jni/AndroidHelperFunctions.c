/*
 * AndroidHelperFunctions.cpp
 *
 *       Created on:  Mar 10, 2015
 *  Last Updated on:  Mar 10, 2015
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2015 Fazecast, Inc.
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

#ifdef __linux__
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include "AndroidHelperFunctions.h"

void push_back(struct charPairVector* vector, const char* firstString, const char* secondString)
{
	// Allocate memory for new string storage
	vector->length++;
	char** newMemory = (char**)realloc(vector->first, vector->length*sizeof(char*));
	if (newMemory)
		vector->first = newMemory;
	newMemory = (char**)realloc(vector->second, vector->length*sizeof(char*));
	if (newMemory)
		vector->second = newMemory;

	// Store new strings
	vector->first[vector->length-1] = (char*)malloc(strlen(firstString)+1);
	vector->second[vector->length-1] = (char*)malloc(strlen(secondString)+1);
	strcpy(vector->first[vector->length-1], firstString);
	strcpy(vector->second[vector->length-1], secondString);
}

void getFriendlyName(const char* productFile, char* friendlyName)
{
	int friendlyNameLength = 0;
	friendlyName[0] = '\0';

	FILE *input = fopen(productFile, "rb");
	if (input)
	{
		char ch = getc(input);
		while ((ch != '\n') && (ch != EOF))
		{
			friendlyName[friendlyNameLength++] = ch;
			ch = getc(input);
		}
		friendlyName[friendlyNameLength] = '\0';
		fclose(input);
	}
}

void recursiveSearchForComPorts(charPairVector* comPorts, const char* fullPathToSearch)
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
					if (friendlyName[0] == '\0')		// Must be a physical platform port
					{
						// Ensure that the platform port is actually open
						struct serial_struct serialInfo = { 0 };
						int fd = open(systemName, O_RDWR | O_NONBLOCK | O_NOCTTY);
						if (fd >= 0)
						{
							if ((ioctl(fd, TIOCGSERIAL, &serialInfo) == 0) && (serialInfo.type != PORT_UNKNOWN))
							{
								strcpy(friendlyName, "Physical Port ");
								strcat(friendlyName, directoryEntry->d_name+3);
								push_back(comPorts, systemName, friendlyName);
							}
							close(fd);
						}
					}
					else
						push_back(comPorts, systemName, friendlyName);

					// Clean up memory
					free(productFile);
					free(systemName);
					free(friendlyName);
				}
				else
				{
					// Search for more serial ports within the directory
					charPairVector newComPorts = { (char**)malloc(1), (char**)malloc(1), 0 };
					char* nextDirectory = (char*)malloc(strlen(fullPathToSearch) + strlen(directoryEntry->d_name) + 5);
					strcpy(nextDirectory, fullPathToSearch);
					strcat(nextDirectory, directoryEntry->d_name);
					strcat(nextDirectory, "/");
					recursiveSearchForComPorts(&newComPorts, nextDirectory);
					free(nextDirectory);
					int i;
					for (i = 0; i < newComPorts.length; ++i)
					{
						push_back(comPorts, newComPorts.first[i], newComPorts.second[i]);
						free(newComPorts.first[i]);
						free(newComPorts.second[i]);
					}
					free(newComPorts.first);
					free(newComPorts.second);
				}
			}
		}
		directoryEntry = readdir(directoryIterator);
	}

	// Close the directory
	closedir(directoryIterator);
}

#endif
