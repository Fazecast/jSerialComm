/*
 * LinuxHelperFunctions.c
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

#ifdef __linux__
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/serial.h>
#include <dirent.h>
#include <asm/termios.h>
#include <asm/ioctls.h>
#include "LinuxHelperFunctions.h"

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

char keyExists(struct charPairVector* vector, const char* key)
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
		while ((ch != '\n') && (ch != EOF))
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
				if ((strlen(directoryEntry->d_name) > 3) &&
						(((directoryEntry->d_name[0] == 't') && (directoryEntry->d_name[1] == 't') && (directoryEntry->d_name[2] == 'y')) ||
								((directoryEntry->d_name[0] == 'r') && (directoryEntry->d_name[1] == 'f') && (directoryEntry->d_name[2] == 'c'))))
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
					if (friendlyName[0] == '\0')		// Must be a physical (or emulated) port
					{
						// See if this is a USB-to-Serial converter based on the driver loaded
						strcpy(productFile, fullPathToSearch);
						strcat(productFile, directoryEntry->d_name);
						strcat(productFile, "/driver/module/drivers");
						getDriverName(productFile, friendlyName);
						if (friendlyName[0] == '\0')	// Must be a physical port
						{
							// Ensure that the platform port is actually open
							struct serial_struct serialInfo = { 0 };
							int fd = open(systemName, O_RDWR | O_NONBLOCK | O_NOCTTY);
							if (fd >= 0)
							{
								if ((strlen(directoryEntry->d_name) >= 6) && (directoryEntry->d_name[0] == 'r') && (directoryEntry->d_name[1] == 'f') && (directoryEntry->d_name[2] == 'c') &&
										(directoryEntry->d_name[3] == 'o') && (directoryEntry->d_name[4] == 'm') && (directoryEntry->d_name[5] == 'm'))
								{
									strcpy(friendlyName, "Bluetooth Port ");
									strcat(friendlyName, directoryEntry->d_name);
									push_back(comPorts, systemName, friendlyName);
								}
								else if (((strlen(directoryEntry->d_name) >= 6) && (directoryEntry->d_name[3] == 'A') && (directoryEntry->d_name[4] == 'M') && (directoryEntry->d_name[5] == 'A')) ||
										((ioctl(fd, TIOCGSERIAL, &serialInfo) == 0) && (serialInfo.type != PORT_UNKNOWN)))
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

void lastDitchSearchForComPorts(charPairVector* comPorts)
{
	// Open the linux dev directory
	DIR *directoryIterator = opendir("/dev/");
	if (!directoryIterator)
		return;

	// Read all files in the current directory
	struct dirent *directoryEntry = readdir(directoryIterator);
	while (directoryEntry)
	{
		// See if the file names a potential serial port
		if ((strlen(directoryEntry->d_name) >= 6) && (directoryEntry->d_name[0] == 't') && (directoryEntry->d_name[1] == 't') && (directoryEntry->d_name[2] == 'y') &&
				(((directoryEntry->d_name[3] == 'A') && (directoryEntry->d_name[4] == 'M') && (directoryEntry->d_name[5] == 'A')) ||
						((directoryEntry->d_name[3] == 'A') && (directoryEntry->d_name[4] == 'C') && (directoryEntry->d_name[5] == 'M')) ||
						((directoryEntry->d_name[3] == 'U') && (directoryEntry->d_name[4] == 'S') && (directoryEntry->d_name[5] == 'B'))))
		{
			// Determine system name of port
			char* systemName = (char*)malloc(256);
			strcpy(systemName, "/dev/");
			strcat(systemName, directoryEntry->d_name);

			// Set static friendly name
			char* friendlyName = (char*)malloc(256);
			strcpy(friendlyName, "USB-Based Serial Port");

			// Determine if port is already in the list, and add it if not
			if (!keyExists(comPorts, systemName))
				push_back(comPorts, systemName, friendlyName);

			// Clean up memory
			free(systemName);
			free(friendlyName);
		}
		else if ((strlen(directoryEntry->d_name) >= 6) && (directoryEntry->d_name[0] == 'r') && (directoryEntry->d_name[1] == 'f') && (directoryEntry->d_name[2] == 'c') &&
				(directoryEntry->d_name[3] == 'o') && (directoryEntry->d_name[4] == 'm') && (directoryEntry->d_name[5] == 'm'))
		{
			// Determine system name of port
			char* systemName = (char*)malloc(256);
			strcpy(systemName, "/dev/");
			strcat(systemName, directoryEntry->d_name);

			// Set static friendly name
			char* friendlyName = (char*)malloc(256);
			strcpy(friendlyName, "Bluetooth-Based Serial Port");

			// Determine if port is already in the list, and add it if not
			if (!keyExists(comPorts, systemName))
				push_back(comPorts, systemName, friendlyName);

			// Clean up memory
			free(systemName);
			free(friendlyName);
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
	struct termios2 options = { 0 };
	ioctl(portFD, TCGETS2, &options);
	options.c_cflag &= ~CBAUD;
	options.c_cflag |= BOTHER;
	options.c_ispeed = baudRate;
	options.c_ospeed = baudRate;
	ioctl(portFD, TCSETS2, &options);
}

#endif
