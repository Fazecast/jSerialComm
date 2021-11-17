/*
 * PosixHelperFunctions.c
 *
 *       Created on:  Mar 10, 2015
 *  Last Updated on:  Nov 14, 2021
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2021 Fazecast, Inc.
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "PosixHelperFunctions.h"

// Common storage functionality
serialPort* pushBack(serialPortVector* vector, const char* key, const char* friendlyName, const char* description)
{
	// Allocate memory for the new SerialPort storage structure
	if (vector->capacity == vector->length)
	{
		serialPort** newArray = (serialPort**)realloc(vector->ports, ++vector->capacity * sizeof(serialPort*));
		if (newArray)
			vector->ports = newArray;
		else
		{
			vector->capacity--;
			return NULL;
		}
	}
	serialPort* port = (serialPort*)malloc(sizeof(serialPort));
	if (port)
		vector->ports[vector->length++] = port;
	else
		return NULL;

	// Initialize the storage structure
	memset(port, 0, sizeof(serialPort));
	port->handle = -1;
	port->enumerated = 1;
	port->portPath = (char*)malloc(strlen(key) + 1);
	port->friendlyName = (char*)malloc(strlen(friendlyName) + 1);
	port->portDescription = (char*)malloc(strlen(description) + 1);

	// Store port strings
	strcpy(port->portPath, key);
	strcpy(port->friendlyName, friendlyName);
	strcpy(port->portDescription, description);

	// Return the newly created serial port structure
	return port;
}

serialPort* fetchPort(serialPortVector* vector, const char* key)
{
	for (int i = 0; i < vector->length; ++i)
		if (strcmp(key, vector->ports[i]->portPath) == 0)
			return vector->ports[i];
	return NULL;
}

void removePort(serialPortVector* vector, serialPort* port)
{
	// Clean up memory associated with the port
	free(port->portPath);
	free(port->friendlyName);
	free(port->portDescription);
	if (port->readBuffer)
		free(port->readBuffer);

	// Move up all remaining ports in the serial port listing
	for (int i = 0; i < vector->length; ++i)
		if (vector->ports[i] == port)
		{
			for (int j = i; j < (vector->length - 1); ++j)
				vector->ports[j] = vector->ports[j+1];
			vector->length--;
			break;
		}

	// Free the serial port structure memory
	free(port);
}

// Linux-specific functionality
#if defined(__linux__)

#include <linux/serial.h>
#include <asm/termios.h>
#include <asm/ioctls.h>

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

void getInterfaceDescription(const char* interfaceFile, char* interfaceDescription)
{
	int interfaceDescriptionLength = 0;
	interfaceDescription[0] = '\0';

	FILE *input = fopen(interfaceFile, "rb");
	if (input)
	{
		char ch = getc(input);
		while ((ch != '\n') && (ch != EOF))
		{
			interfaceDescription[interfaceDescriptionLength++] = ch;
			ch = getc(input);
		}
		interfaceDescription[interfaceDescriptionLength] = '\0';
		fclose(input);
	}
}

void recursiveSearchForComPorts(serialPortVector* comPorts, const char* fullPathToSearch)
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

					// Check if port is already enumerated
					serialPort *port = fetchPort(comPorts, systemName);
					if (port)
						port->enumerated = 1;
					else
					{
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
										pushBack(comPorts, systemName, friendlyName, friendlyName);
									}
									else if (((strlen(directoryEntry->d_name) >= 6) && (directoryEntry->d_name[3] == 'A') && (directoryEntry->d_name[4] == 'M') && (directoryEntry->d_name[5] == 'A')) ||
											((ioctl(fd, TIOCGSERIAL, &serialInfo) == 0) && (serialInfo.type != PORT_UNKNOWN)))
									{
										strcpy(friendlyName, "Physical Port ");
										strcat(friendlyName, directoryEntry->d_name+3);
										pushBack(comPorts, systemName, friendlyName, friendlyName);
									}
									close(fd);
								}
							}
							else
							{
								// Attempt to read from the device interface file
								char* interfaceDescription = (char*)malloc(256);
								char* interfaceFile = (char*)malloc(strlen(fullPathToSearch) + strlen(directoryEntry->d_name) + 30);
								strcpy(interfaceFile, fullPathToSearch);
								strcat(interfaceFile, directoryEntry->d_name);
								strcat(interfaceFile, "/../interface");
								getInterfaceDescription(interfaceFile, interfaceDescription);
								if (interfaceDescription[0] == '\0')
								{
									strcpy(interfaceFile, fullPathToSearch);
									strcat(interfaceFile, directoryEntry->d_name);
									strcat(interfaceFile, "/device/../interface");
									getInterfaceDescription(interfaceFile, interfaceDescription);
								}
								if (interfaceDescription[0] == '\0')
									strcpy(interfaceDescription, friendlyName);
								pushBack(comPorts, systemName, friendlyName, interfaceDescription);

								// Clean up memory
								free(interfaceFile);
								free(interfaceDescription);
							}
						}
						else
						{
							// Attempt to read from the device interface file
							char* interfaceDescription = (char*)malloc(256);
							char* interfaceFile = (char*)malloc(strlen(fullPathToSearch) + strlen(directoryEntry->d_name) + 30);
							strcpy(interfaceFile, fullPathToSearch);
							strcat(interfaceFile, directoryEntry->d_name);
							strcat(interfaceFile, "/../interface");
							getInterfaceDescription(interfaceFile, interfaceDescription);
							if (interfaceDescription[0] == '\0')
							{
								strcpy(interfaceFile, fullPathToSearch);
								strcat(interfaceFile, directoryEntry->d_name);
								strcat(interfaceFile, "/device/../interface");
								getInterfaceDescription(interfaceFile, interfaceDescription);
							}
							if (interfaceDescription[0] == '\0')
								strcpy(interfaceDescription, friendlyName);
							pushBack(comPorts, systemName, friendlyName, interfaceDescription);

							// Clean up memory
							free(interfaceFile);
							free(interfaceDescription);
						}

						// Clean up memory
						free(productFile);
						free(friendlyName);
					}

					// Clean up memory
					free(systemName);
				}
				else
				{
					// Search for more serial ports within the directory
					char* nextDirectory = (char*)malloc(strlen(fullPathToSearch) + strlen(directoryEntry->d_name) + 5);
					strcpy(nextDirectory, fullPathToSearch);
					strcat(nextDirectory, directoryEntry->d_name);
					strcat(nextDirectory, "/");
					recursiveSearchForComPorts(comPorts, nextDirectory);
					free(nextDirectory);
				}
			}
		}
		directoryEntry = readdir(directoryIterator);
	}

	// Close the directory
	closedir(directoryIterator);
}

void driverBasedSearchForComPorts(serialPortVector* comPorts, const char* fullPathToDriver, const char* fullBasePathToPort)
{
	// Search for unidentified physical serial ports
	FILE *serialDriverFile = fopen(fullPathToDriver, "rb");
	if (serialDriverFile)
	{
		char* serialLine = (char*)malloc(128);
		while (fgets(serialLine, 128, serialDriverFile))
			if (strstr(serialLine, "uart:") && (strstr(serialLine, "uart:unknown") == NULL))
			{
				// Determine system name of port
				*strchr(serialLine, ':') = '\0';
				char* systemName = (char*)malloc(256);
				strcpy(systemName, fullBasePathToPort);
				strcat(systemName, serialLine);

				// Check if port is already enumerated
				serialPort *port = fetchPort(comPorts, systemName);
				if (port)
					port->enumerated = 1;
				else
				{
					// Ensure that the port is valid and not a symlink
					struct stat fileStats;
					if ((access(systemName, F_OK) == 0) && (lstat(systemName, &fileStats) == 0) && !S_ISLNK(fileStats.st_mode))
					{
						// Set static friendly name
						char* friendlyName = (char*)malloc(256);
						strcpy(friendlyName, "Physical Port ");
						strcat(friendlyName, serialLine);

						// Add the port to the list
						pushBack(comPorts, systemName, friendlyName, friendlyName);

						// Clean up memory
						free(friendlyName);
					}
				}

				// Clean up memory
				free(systemName);
			}
		free(serialLine);
		fclose(serialDriverFile);
	}
}

void lastDitchSearchForComPorts(serialPortVector* comPorts)
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

			// Check if port is already enumerated
			serialPort *port = fetchPort(comPorts, systemName);
			if (port)
				port->enumerated = 1;
			else
			{
				// Set static friendly name
				char* friendlyName = (char*)malloc(256);
				strcpy(friendlyName, "USB-Based Serial Port");

				// Add the port to the list
				pushBack(comPorts, systemName, friendlyName, friendlyName);

				// Clean up memory
				free(friendlyName);
			}

			// Clean up memory
			free(systemName);
		}
		else if ((strlen(directoryEntry->d_name) >= 6) && (directoryEntry->d_name[0] == 't') && (directoryEntry->d_name[1] == 't') && (directoryEntry->d_name[2] == 'y') &&
				(directoryEntry->d_name[3] == 'A') && (directoryEntry->d_name[4] == 'P'))
		{
			// Determine system name of port
			char* systemName = (char*)malloc(256);
			strcpy(systemName, "/dev/");
			strcat(systemName, directoryEntry->d_name);

			// Check if port is already enumerated
			serialPort *port = fetchPort(comPorts, systemName);
			if (port)
				port->enumerated = 1;
			else
			{
				// Set static friendly name
				char* friendlyName = (char*)malloc(256);
				strcpy(friendlyName, "Advantech Extended Serial Port");

				// Add the port to the list
				pushBack(comPorts, systemName, friendlyName, friendlyName);

				// Clean up memory
				free(friendlyName);
			}

			// Clean up memory
			free(systemName);
		}
		else if ((strlen(directoryEntry->d_name) >= 6) && (directoryEntry->d_name[0] == 'r') && (directoryEntry->d_name[1] == 'f') && (directoryEntry->d_name[2] == 'c') &&
				(directoryEntry->d_name[3] == 'o') && (directoryEntry->d_name[4] == 'm') && (directoryEntry->d_name[5] == 'm'))
		{
			// Determine system name of port
			char* systemName = (char*)malloc(256);
			strcpy(systemName, "/dev/");
			strcat(systemName, directoryEntry->d_name);

			// Check if port is already enumerated
			serialPort *port = fetchPort(comPorts, systemName);
			if (port)
				port->enumerated = 1;
			else
			{
				// Set static friendly name
				char* friendlyName = (char*)malloc(256);
				strcpy(friendlyName, "Bluetooth-Based Serial Port");

				// Add the port to the list
				pushBack(comPorts, systemName, friendlyName, friendlyName);

				// Clean up memory
				free(friendlyName);
			}

			// Clean up memory
			free(systemName);
		}
		directoryEntry = readdir(directoryIterator);
	}

	// Close the directory
	closedir(directoryIterator);
}

baud_rate getBaudRateCode(baud_rate baudRate)
{
	// Translate a raw baud rate into a system-specified one
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
#ifdef B57600
			return B57600;
#else
			return 0;
#endif
		case 76800:
#ifdef B76800
			return B76800;
#else
			return 0;
#endif
		case 115200:
#ifdef B115200
			return B115200;
#else
			return 0;
#endif
		case 153600:
#ifdef B153600
			return B153600;
#else
			return 0;
#endif
		case 230400:
#ifdef B230400
			return B230400;
#else
			return 0;
#endif
		case 307200:
#ifdef B307200
			return B307200;
#else
			return 0;
#endif
		case 460800:
#ifdef B460800
			return B460800;
#else
			return 0;
#endif
		case 500000:
#ifdef B500000
			return B500000;
#else
			return 0;
#endif
		case 576000:
#ifdef B576000
			return B576000;
#else
			return 0;
#endif
		case 614400:
#ifdef B614400
			return B614400;
#else
			return 0;
#endif
		case 921600:
#ifdef B921600
			return B921600;
#else
			return 0;
#endif
		case 1000000:
#ifdef B1000000
			return B1000000;
#else
			return 0;
#endif
		case 1152000:
#ifdef B1152000
			return B1152000;
#else
			return 0;
#endif
		case 1500000:
#ifdef B1500000
			return B1500000;
#else
			return 0;
#endif
		case 2000000:
#ifdef B2000000
			return B2000000;
#else
			return 0;
#endif
		case 2500000:
#ifdef B2500000
			return B2500000;
#else
			return 0;
#endif
		case 3000000:
#ifdef B3000000
			return B3000000;
#else
			return 0;
#endif
		case 3500000:
#ifdef B3500000
			return B3500000;
#else
			return 0;
#endif
		case 4000000:
#ifdef B4000000
			return B4000000;
#else
			return 0;
#endif
		default:
			return 0;
	}

	return 0;
}

int setBaudRateCustom(int portFD, baud_rate baudRate)
{
#ifdef TCSETS2
	struct termios2 options = { 0 };
	ioctl(portFD, TCGETS2, &options);
	options.c_cflag &= ~CBAUD;
	options.c_cflag |= BOTHER;
	options.c_ispeed = baudRate;
	options.c_ospeed = baudRate;
	int retVal = ioctl(portFD, TCSETS2, &options);
#else
	struct serial_struct serInfo;
	int retVal = ioctl(portFD, TIOCGSERIAL, &serInfo);
	if (retVal == 0)
	{
		serInfo.flags &= ~ASYNC_SPD_MASK;
		serInfo.flags |= ASYNC_SPD_CUST | ASYNC_LOW_LATENCY;
		serInfo.custom_divisor = serInfo.baud_base / baudRate;
		if (serInfo.custom_divisor == 0)
			serInfo.custom_divisor = 1;
		retVal = ioctl(portFD, TIOCSSERIAL, &serInfo);
	}
#endif
	return retVal;
}

// Solaris-specific functionality
#elif defined(__sun__)

#include <termios.h>

void searchForComPorts(serialPortVector* comPorts)
{
	// Open the Solaris callout dev directory
	DIR *directoryIterator = opendir("/dev/cua/");
	if (directoryIterator)
	{
		// Read all files in the current directory
		struct dirent *directoryEntry = readdir(directoryIterator);
		while (directoryEntry)
		{
			// See if the file names a potential serial port
			if ((strlen(directoryEntry->d_name) >= 1) && (directoryEntry->d_name[0] != '.'))
			{
				// Determine system name of port
				char* systemName = (char*)malloc(256);
				strcpy(systemName, "/dev/cua/");
				strcat(systemName, directoryEntry->d_name);

				// Check if port is already enumerated
				serialPort *port = fetchPort(comPorts, systemName);
				if (port)
					port->enumerated = 1;
				else
				{
					// Set static friendly name
					char* friendlyName = (char*)malloc(256);
					strcpy(friendlyName, "Serial Port");

					// Add the port to the list if it is not a directory
					struct stat fileStats;
					stat(systemName, &fileStats);
					if (!S_ISDIR(fileStats.st_mode))
						pushBack(comPorts, systemName, friendlyName, friendlyName);

					// Clean up memory
					free(friendlyName);
				}

				// Clean up memory
				free(systemName);
			}
			directoryEntry = readdir(directoryIterator);
		}

		// Close the directory
		closedir(directoryIterator);
	}

	// Open the Solaris dial-in dev directory
	directoryIterator = opendir("/dev/term/");
	if (directoryIterator)
	{
		// Read all files in the current directory
		struct dirent *directoryEntry = readdir(directoryIterator);
		while (directoryEntry)
		{
			// See if the file names a potential serial port
			if ((strlen(directoryEntry->d_name) >= 1) && (directoryEntry->d_name[0] != '.'))
			{
				// Determine system name of port
				char* systemName = (char*)malloc(256);
				strcpy(systemName, "/dev/term/");
				strcat(systemName, directoryEntry->d_name);

				// Check if port is already enumerated
				serialPort *port = fetchPort(comPorts, systemName);
				if (port)
					port->enumerated = 1;
				else
				{
					// Set static friendly name
					char* friendlyName = (char*)malloc(256);
					strcpy(friendlyName, "Serial Port (Dial-In)");

					// Add the port to the list if the file is not a directory
					struct stat fileStats;
					stat(systemName, &fileStats);
					if (!S_ISDIR(fileStats.st_mode))
						pushBack(comPorts, systemName, friendlyName, friendlyName);

					// Clean up memory
					free(friendlyName);
				}

				// Clean up memory
				free(systemName);
			}
			directoryEntry = readdir(directoryIterator);
		}

		// Close the directory
		closedir(directoryIterator);
	}
}

int flock(int fd, int op)
{
	int rc = 0;

#if defined(F_SETLK) && defined(F_SETLKW)
	struct flock fl = { 0 };
	switch (op & (LOCK_EX | LOCK_SH | LOCK_UN))
	{
		case LOCK_EX:
			fl.l_type = F_WRLCK;
			break;
		case LOCK_SH:
			fl.l_type = F_RDLCK;
			break;
		case LOCK_UN:
			fl.l_type = F_UNLCK;
			break;
		default:
			errno = EINVAL;
			return -1;
	}
	fl.l_whence = SEEK_SET;
	rc = fcntl(fd, op & LOCK_NB ? F_SETLK : F_SETLKW, &fl);
	if (rc && (errno == EAGAIN))
		errno = EWOULDBLOCK;
#endif

	return rc;
}

baud_rate getBaudRateCode(baud_rate baudRate)
{
	// Translate a raw baud rate into a system-specified one
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
		case 76800:
			return B76800;
		case 115200:
			return B115200;
		case 153600:
			return B153600;
		case 230400:
			return B230400;
		case 307200:
			return B307200;
		case 460800:
			return B460800;
		default:
			return 0;
	}

	return 0;
}

int setBaudRateCustom(int portFD, baud_rate baudRate)
{
	// No way to set custom baud rates on this OS
	return -1;
}

// Apple-specific functionality
#elif defined(__APPLE__)

#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/ioss.h>
#include <sys/ioctl.h>

void searchForComPorts(serialPortVector* comPorts)
{
	serialPort *port;
	int numValues = 0;
	io_object_t serialPort;
	io_iterator_t serialPortIterator;
	char friendlyName[1024], comPortCu[1024], comPortTty[1024], portDescription[1024];

	// Enumerate serial ports on machine
	IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching(kIOSerialBSDServiceValue), &serialPortIterator);
	while ((serialPort = IOIteratorNext(serialPortIterator)))
	{
		++numValues;
		IOObjectRelease(serialPort);
	}
	IOIteratorReset(serialPortIterator);
	for (int i = 0; i < numValues; ++i)
	{
		// Get serial port information
		friendlyName[0] = '\0';
		serialPort = IOIteratorNext(serialPortIterator);
		io_registry_entry_t parent = 0;
		io_registry_entry_t service = serialPort;
		while (service)
		{
			if (IOObjectConformsTo(service, "IOUSBDevice"))
			{
				IORegistryEntryGetName(service, friendlyName);
				break;
			}
			if (IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent) != KERN_SUCCESS)
				break;
			if (service != serialPort)
				IOObjectRelease(service);
			service = parent;
		}
		if (service != serialPort)
			IOObjectRelease(service);

		// Get serial port name and COM value
		if (friendlyName[0] == '\0')
		{
			CFStringRef friendlyNameRef = (CFStringRef)IORegistryEntryCreateCFProperty(serialPort, CFSTR(kIOTTYDeviceKey), kCFAllocatorDefault, 0);
			CFStringGetCString(friendlyNameRef, friendlyName, sizeof(friendlyName), kCFStringEncodingUTF8);
			CFRelease(friendlyNameRef);
		}
		CFStringRef comPortRef = (CFStringRef)IORegistryEntryCreateCFProperty(serialPort, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0);
		CFStringGetCString(comPortRef, comPortCu, sizeof(comPortCu), kCFStringEncodingUTF8);
		CFRelease(comPortRef);
		comPortRef = (CFStringRef)IORegistryEntryCreateCFProperty(serialPort, CFSTR(kIODialinDeviceKey), kCFAllocatorDefault, 0);
		CFStringGetCString(comPortRef, comPortTty, sizeof(comPortTty), kCFStringEncodingUTF8);
		CFRelease(comPortRef);

		// Check if callout port is already enumerated
		port = fetchPort(comPorts, comPortCu);
		if (port)
			port->enumerated = 1;
		else
			pushBack(comPorts, comPortCu, friendlyName, friendlyName);

		// Check if dialin port is already enumerated
		port = fetchPort(comPorts, comPortTty);
		strcat(friendlyName, " (Dial-In)");
		if (port)
			port->enumerated = 1;
		else
			pushBack(comPorts, comPortTty, friendlyName, friendlyName);
		IOObjectRelease(serialPort);
	}
	IOObjectRelease(serialPortIterator);
}

baud_rate getBaudRateCode(baud_rate baudRate)
{
	// Translate a raw baud rate into a system-specified one
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
		case 7200:
#ifdef B7200
			return B7200;
#else
			return 0;
#endif
		case 14400:
#ifdef B14400
			return B14400;
#else
			return 0;
#endif
		case 28800:
#ifdef B28800
			return B28800;
#else
			return 0;
#endif
		case 57600:
#ifdef B57600
			return B57600;
#else
			return 0;
#endif
		case 76800:
#ifdef B76800
			return B76800;
#else
			return 0;
#endif
		case 115200:
#ifdef B115200
			return B115200;
#else
			return 0;
#endif
		case 230400:
#ifdef B230400
			return B230400;
#else
			return 0;
#endif
		default:
			return 0;
	}

	return 0;
}

int setBaudRateCustom(int portFD, baud_rate baudRate)
{
	// Use OSX-specific ioctls to set a custom baud rate
	unsigned long microseconds = 1000;
	int retVal = ioctl(portFD, IOSSIOSPEED, &baudRate);
	if (retVal == 0)
		retVal = ioctl(portFD, IOSSDATALAT, &microseconds);
	return retVal;
}

#endif
