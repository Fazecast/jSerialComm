/*
 * PosixHelperFunctions.c
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "PosixHelperFunctions.h"

// Common serial port storage functionality
serialPort* pushBack(serialPortVector* vector, const char* key, const char* friendlyName, const char* description, const char* location, int vid, int pid)
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

	// Initialize the serial port mutex and condition variables
	memset(port, 0, sizeof(serialPort));
	pthread_mutex_init(&port->eventMutex, NULL);
	pthread_condattr_t conditionVariableAttributes;
	pthread_condattr_init(&conditionVariableAttributes);
#if !defined(__APPLE__) && !defined(__OpenBSD__) && !defined(__ANDROID__)
	pthread_condattr_setclock(&conditionVariableAttributes, CLOCK_REALTIME);
#endif
	pthread_cond_init(&port->eventReceived, &conditionVariableAttributes);
	pthread_condattr_destroy(&conditionVariableAttributes);

	// Initialize the storage structure
	port->handle = -1;
	port->enumerated = 1;
	port->vendorID = vid;
	port->productID = pid;
	port->portPath = (char*)malloc(strlen(key) + 1);
	port->portLocation = (char*)malloc(strlen(location) + 1);
	port->friendlyName = (char*)malloc(strlen(friendlyName) + 1);
	port->portDescription = (char*)malloc(strlen(description) + 1);

	// Store port strings
	strcpy(port->portPath, key);
	strcpy(port->portLocation, location);
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
	free(port->portLocation);
	free(port->friendlyName);
	free(port->portDescription);
	if (port->readBuffer)
		free(port->readBuffer);
	pthread_cond_destroy(&port->eventReceived);
	pthread_mutex_destroy(&port->eventMutex);

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

void cleanUpVector(serialPortVector* vector)
{
	while (vector->length)
		removePort(vector, vector->ports[0]);
	if (vector->ports)
		free(vector->ports);
	vector->ports = NULL;
	vector->length = vector->capacity = 0;
}

// Common string storage functionality
typedef struct stringVector
{
   char **strings;
   int length, capacity;
} stringVector;

static void pushBackString(stringVector* vector, const char* string)
{
	// Allocate memory for the new port path prefix storage structure
	if (vector->capacity == vector->length)
	{
		char** newStringArray = (char**)realloc(vector->strings, ++vector->capacity * sizeof(const char*));
		if (newStringArray)
			vector->strings = newStringArray;
		else
		{
			vector->capacity--;
			return;
		}
	}
	char* newString = (char*)malloc(strlen(string) + 1);
	if (newString)
	{
		strcpy(newString, string);
		vector->strings[vector->length++] = newString;
	}
}

static void freeStringVector(stringVector* vector)
{
	for (int i = 0; i < vector->length; ++i)
		free(vector->strings[i]);
	if (vector->strings)
		free(vector->strings);
	vector->strings = NULL;
	vector->length = vector->capacity = 0;
}

// Linux-specific functionality
#if defined(__linux__)

#include <linux/serial.h>
#include <asm/termios.h>
#include <asm/ioctls.h>

static void retrievePhysicalPortPrefixes(stringVector* prefixes)
{
	// Open the TTY drivers file
	FILE *input = fopen("/proc/tty/drivers", "rb");
	if (input)
	{
		// Read the file line by line
		int maxLineSize = 256;
		char *line = (char*)malloc(maxLineSize);
		while (fgets(line, maxLineSize, input))
		{
			// Parse the prefix, name, and type fields
			int i = 0;
			char *token, *remainder = line, *prefix = NULL, *name = NULL, *type = NULL;
			while ((token = strtok_r(remainder, " \t\r\n", &remainder)))
				if (++i == 1)
					name = token;
				else if (i == 2)
					prefix = token;
				else if (i == 5)
					type = token;

			// Add a new prefix to the vector if the driver type and name are both "serial"
			if (prefix && type && name && (strcmp(type, "serial") == 0) && (strcmp(name, "serial") == 0))
				pushBackString(prefixes, prefix);
		}

		// Close the drivers file and clean up memory
		free(line);
		fclose(input);
	}
}

static char isUsbSerialSubsystem(const char *subsystemLink)
{
	// Attempt to read the path that the subsystem link points to
	struct stat fileInfo;
	char *subsystem = NULL, isUsbSerial = 0;
	if (lstat(subsystemLink, &fileInfo) == 0)
	{
		ssize_t bufferSize = fileInfo.st_size ? (fileInfo.st_size + 1) : PATH_MAX;
		subsystem = (char*)malloc(bufferSize);
		if (subsystem)
		{
			ssize_t subsystemSize = readlink(subsystemLink, subsystem, bufferSize);
			if (subsystemSize >= 0)
			{
				subsystem[subsystemSize] = '\0';
				if (strcmp(1 + strrchr(subsystem, '/'), "usb-serial") == 0)
					isUsbSerial = 1;
			}
			free(subsystem);
		}
	}
	return isUsbSerial;
}

static void getPortLocation(const char* portDirectory, char* portLocation, int physicalPortNumber)
{
	// Set location of busnum and devpath files
	char* busnumFile = (char*)malloc(strlen(portDirectory) + 16);
	strcpy(busnumFile, portDirectory);
	strcat(busnumFile, "busnum");
	char* devpathFile = (char*)malloc(strlen(portDirectory) + 16);
	strcpy(devpathFile, portDirectory);
	strcat(devpathFile, "devpath");
	int portLocationLength = 0;
	portLocation[0] = '\0';

	// Read the bus number
	FILE *input = fopen(busnumFile, "rb");
	if (input)
	{
		int ch = getc(input);
		while (((char)ch != '\n') && (ch != EOF))
		{
			portLocation[portLocationLength++] = (char)ch;
			ch = getc(input);
		}
		portLocation[portLocationLength++] = '-';
		fclose(input);
	}
	else
	{
		portLocation[portLocationLength++] = '0';
		portLocation[portLocationLength++] = '-';
	}

	// Read the device path
	input = fopen(devpathFile, "rb");
	if (input)
	{
		int ch = getc(input);
		while (((char)ch != '\n') && (ch != EOF))
		{
			portLocation[portLocationLength++] = (char)ch;
			ch = getc(input);
		}
		portLocation[portLocationLength] = '\0';
		fclose(input);
	}
	else
	{
		portLocation[portLocationLength++] = '0';
		portLocation[portLocationLength] = '\0';
	}

	// Append the physical port number if applicable
	if (physicalPortNumber >= 0)
		sprintf(portLocation + portLocationLength, ".%d", physicalPortNumber);

	// Clean up the dynamic memory
	free(devpathFile);
	free(busnumFile);
}

static void assignFriendlyName(const char* portDevPath, char* friendlyName)
{
	// Manually assign a friendly name if the port type is known from its name
	const char *portName = 1 + strrchr(portDevPath, '/');
	if ((strlen(portName) >= 5) && (portName[3] == 'A') && (portName[4] == 'P'))
		strcpy(friendlyName, "Advantech Extended Serial Port");
	else if ((strlen(portName) >= 6) && (portName[0] == 'r') && (portName[1] == 'f') && (portName[2] == 'c') &&
			(portName[3] == 'o') && (portName[4] == 'm') && (portName[5] == 'm'))
		strcpy(friendlyName, "Bluetooth-Based Serial Port");
	else
	{
		// Assign a friendly name based on the serial port driver in use
		char nameAssigned = 0;
		FILE *input = fopen("/proc/tty/drivers", "rb");
		if (input)
		{
			// Read the TTY drivers file line by line
			int maxLineSize = 256;
			char *line = (char*)malloc(maxLineSize);
			while (!nameAssigned && fgets(line, maxLineSize, input))
			{
				// Parse the prefix, name, and type fields
				int i = 0;
				char *token, *remainder = line, *prefix = NULL, *name = NULL, *type = NULL;
				while ((token = strtok_r(remainder, " \t\r\n", &remainder)))
					if (++i == 1)
						name = token;
					else if (i == 2)
						prefix = token;
					else if (i == 5)
						type = token;

				// Assign a friendly name if a matching port prefix was found
				if (prefix && name && type && (strcmp(type, "serial") == 0) && (strstr(portDevPath, prefix) == portDevPath))
				{
					strcpy(friendlyName, "Serial Device (");
					strcat(friendlyName, name);
					strcat(friendlyName, ")");
					nameAssigned = 1;
				}
			}

			// Close the drivers file and clean up memory
			free(line);
			fclose(input);
		}

		// If no driver prefix could be found, just assign a generic friendly name
		if (!nameAssigned)
			strcpy(friendlyName, "USB-Based Serial Port");
	}
}

static void getUsbDetails(const char* fileName, char* basePathEnd, int* vendorID, int* productID)
{
	// Attempt to read the Vendor ID
	char *temp = (char*)malloc(8);
	sprintf(basePathEnd, "../idVendor");
	FILE *input = fopen(fileName, "rb");
	if (input)
	{
		fgets(temp, 8, input);
		*vendorID = (int)strtol(temp, NULL, 16);
		fclose(input);
	}

	// Attempt to read the Product ID
	sprintf(basePathEnd, "../idProduct");
	input = fopen(fileName, "rb");
	if (input)
	{
		fgets(temp, 8, input);
		*productID = (int)strtol(temp, NULL, 16);
		fclose(input);
	}
	free(temp);
}

void searchForComPorts(serialPortVector* comPorts)
{
	// Set up the necessary local variables
	struct stat pathInfo = { 0 };
	stringVector physicalPortPrefixes = { NULL, 0, 0 };
	int fileNameMaxLength = 0, portDevPathMaxLength = 128, maxLineSize = 256;
	char *fileName = NULL, *portDevPath = (char*)malloc(portDevPathMaxLength);
	char *line = (char*)malloc(maxLineSize), *friendlyName = (char*)malloc(256);
	char *portLocation = (char*)malloc(128), *interfaceDescription = (char*)malloc(256);

	// Retrieve the list of /dev/ prefixes for all physical serial ports
	retrievePhysicalPortPrefixes(&physicalPortPrefixes);

	// Iterate through the system TTY directory
	DIR *directoryIterator = opendir("/sys/class/tty/");
	if (directoryIterator)
	{
		struct dirent *directoryEntry = readdir(directoryIterator);
		while (directoryEntry)
		{
			// Ensure that the file name buffer is large enough
			if ((64 + strlen(directoryEntry->d_name)) > fileNameMaxLength)
			{
				fileNameMaxLength = 64 + strlen(directoryEntry->d_name);
				fileName = (char*)realloc(fileName, fileNameMaxLength);
			}

			// Check if the entry represents a valid serial port
			strcpy(fileName, "/sys/class/tty/");
			strcat(fileName, directoryEntry->d_name);
			char *basePathEnd = fileName + strlen(fileName);
			sprintf(basePathEnd, "/device/driver");
			char isSerialPort = (stat(fileName, &pathInfo) == 0) && S_ISDIR(pathInfo.st_mode);
			sprintf(basePathEnd, "/dev");
			isSerialPort = isSerialPort && (stat(fileName, &pathInfo) == 0) && S_ISREG(pathInfo.st_mode);
			sprintf(basePathEnd, "/uevent");
			isSerialPort = isSerialPort && (stat(fileName, &pathInfo) == 0) && S_ISREG(pathInfo.st_mode);
			if (!isSerialPort)
			{
				directoryEntry = readdir(directoryIterator);
				continue;
			}

			// Determine the /dev/ path to the device
			isSerialPort = 0;
			FILE *input = fopen(fileName, "r");
			if (input)
			{
				while (fgets(line, maxLineSize, input))
					if (strstr(line, "DEVNAME=") == line)
					{
						isSerialPort = 1;
						strcpy(portDevPath, "/dev/");
						strcat(portDevPath, line + 8);
						portDevPath[strcspn(portDevPath, "\r\n")] = '\0';
					}
				fclose(input);
			}
			if (!isSerialPort)
			{
				directoryEntry = readdir(directoryIterator);
				continue;
			}

			// Check if the device is a physical serial port
			char isPhysical = 0;
			int physicalPortNumber = 0;
			for (int i = 0; !isPhysical && (i < physicalPortPrefixes.length); ++i)
				if (strstr(portDevPath, physicalPortPrefixes.strings[i]) == portDevPath)
				{
					isPhysical = 1;
					physicalPortNumber = atoi(portDevPath + strlen(physicalPortPrefixes.strings[i]));
				}

			// Determine the subsystem and bus location of the port
			int vendorID = -1, productID = -1;
			sprintf(basePathEnd, "/device/subsystem");
			if (isUsbSerialSubsystem(fileName))
			{
				sprintf(basePathEnd, "/device/../");
				basePathEnd += 11;
			}
			else
			{
				sprintf(basePathEnd, "/device/");
				basePathEnd += 8;
			}
			getUsbDetails(fileName, basePathEnd, &vendorID, &productID);
			sprintf(basePathEnd, "../");
			getPortLocation(fileName, portLocation, isPhysical ? physicalPortNumber : -1);

			// Check if the port has already been enumerated
			serialPort *port = fetchPort(comPorts, portDevPath);
			if (port)
			{
				// See if the device has changed locations
				int oldLength = strlen(port->portLocation);
				int newLength = strlen(portLocation);
				if (oldLength != newLength)
				{
					port->portLocation = (char*)realloc(port->portLocation, newLength + 1);
					strcpy(port->portLocation, portLocation);
				}
				else if (memcmp(port->portLocation, portLocation, newLength))
					strcpy(port->portLocation, portLocation);

				// Update descriptors if this is not a physical port
				if (!isPhysical)
				{
					// Update the device's registered friendly name
					friendlyName[0] = '\0';
					sprintf(basePathEnd, "../product");
					FILE *input = fopen(fileName, "rb");
					if (input)
					{
						fgets(friendlyName, 256, input);
						friendlyName[strcspn(friendlyName, "\r\n")] = '\0';
						fclose(input);
					}
					if (friendlyName[0] == '\0')
						assignFriendlyName(portDevPath, friendlyName);
					oldLength = strlen(port->friendlyName);
					newLength = strlen(friendlyName);
					if (oldLength != newLength)
					{
						port->friendlyName = (char*)realloc(port->friendlyName, newLength + 1);
						strcpy(port->friendlyName, friendlyName);
					}
					else if (memcmp(port->friendlyName, friendlyName, newLength))
						strcpy(port->friendlyName, friendlyName);

					// Attempt to read the bus-reported device description
					interfaceDescription[0] = '\0';
					sprintf(basePathEnd, "interface");
					input = fopen(fileName, "rb");
					if (input)
					{
						fgets(interfaceDescription, 256, input);
						interfaceDescription[strcspn(interfaceDescription, "\r\n")] = '\0';
						fclose(input);
					}
					if (interfaceDescription[0] == '\0')
						strcpy(interfaceDescription, friendlyName);
					oldLength = strlen(port->portDescription);
					newLength = strlen(interfaceDescription);
					if (oldLength != newLength)
					{
						port->portDescription = (char*)realloc(port->portDescription, newLength + 1);
						strcpy(port->portDescription, interfaceDescription);
					}
					else if (memcmp(port->portDescription, interfaceDescription, newLength))
						strcpy(port->portDescription, interfaceDescription);
				}

				// Continue port enumeration
				directoryEntry = readdir(directoryIterator);
				port->enumerated = 1;
				continue;
			}

			// Retrieve all available port details based on its type
			if (isPhysical)
			{
				// Probe the physical port to see if it actually exists
				int fd = open(portDevPath, O_RDWR | O_NONBLOCK | O_NOCTTY);
				if (fd >= 0)
				{
					struct serial_struct serialInfo = { 0 };
					if ((ioctl(fd, TIOCGSERIAL, &serialInfo) == 0) && (serialInfo.type != PORT_UNKNOWN))
					{
						// Add the port to the list of available ports
						strcpy(friendlyName, "Physical Port ");
						strcat(friendlyName, directoryEntry->d_name+3);
						pushBack(comPorts, portDevPath, friendlyName, friendlyName, portLocation, -1, -1);
					}
					close(fd);
				}
			}
			else		// Emulated serial port
			{
				// See if the device has a registered friendly name
				friendlyName[0] = '\0';
				sprintf(basePathEnd, "../product");
				FILE *input = fopen(fileName, "rb");
				if (input)
				{
					fgets(friendlyName, 256, input);
					friendlyName[strcspn(friendlyName, "\r\n")] = '\0';
					fclose(input);
				}
				if (friendlyName[0] == '\0')
					assignFriendlyName(portDevPath, friendlyName);

				// Attempt to read the bus-reported device description
				interfaceDescription[0] = '\0';
				sprintf(basePathEnd, "interface");
				input = fopen(fileName, "rb");
				if (input)
				{
					fgets(interfaceDescription, 256, input);
					interfaceDescription[strcspn(interfaceDescription, "\r\n")] = '\0';
					fclose(input);
				}
				if (interfaceDescription[0] == '\0')
					strcpy(interfaceDescription, friendlyName);

				// Add the port to the list of available ports
				pushBack(comPorts, portDevPath, friendlyName, interfaceDescription, portLocation, vendorID, productID);
			}

			// Read next TTY directory entry
			directoryEntry = readdir(directoryIterator);
		}
		closedir(directoryIterator);
	}

	// Clean up dynamically allocated memory
	freeStringVector(&physicalPortPrefixes);
	if (fileName)
		free(fileName);
	free(interfaceDescription);
	free(portLocation);
	free(friendlyName);
	free(portDevPath);
	free(line);
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
						pushBack(comPorts, systemName, friendlyName, friendlyName, "0-0", -1, -1);

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
						pushBack(comPorts, systemName, friendlyName, friendlyName, "0-0", -1, -1);

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

// FreeBSD-specific functionality
#elif defined(__FreeBSD__)

char getPortDetails(const char *deviceName, char* portLocation, int* vendorID, int* productID)
{
	// Attempt to locate the device in sysctl
	size_t bufferSize = 1024;
	char *stdOutResult = (char*)malloc(bufferSize), *deviceLocation = NULL, *deviceInfo = NULL;
	snprintf(stdOutResult, bufferSize, "sysctl -a | grep \"ttyname: %s\"", deviceName);
	FILE *pipe = popen(stdOutResult, "r");
	if (pipe)
	{
		while (!deviceLocation && fgets(stdOutResult, bufferSize, pipe))
		{
			deviceLocation = stdOutResult;
			*(strstr(deviceLocation, "ttyname:") - 1) = '\0';
			deviceInfo = (char*)malloc(strlen(deviceLocation) + 12);
			strcpy(deviceInfo, deviceLocation);
			strcat(deviceLocation, ".%location");
			strcat(deviceInfo, ".%pnpinfo");
		}
		pclose(pipe);
	}

	// Parse port location
	if (deviceLocation)
	{
		char *temp = (char*)malloc(64);
		sprintf(portLocation, "sysctl -a | grep \"%s\"", deviceLocation);
		pipe = popen(portLocation, "r");
		strcpy(portLocation, "0-0");
		if (pipe)
		{
			while (fgets(stdOutResult, bufferSize, pipe))
				if (strstr(stdOutResult, "bus") && strstr(stdOutResult, "hubaddr") && strstr(stdOutResult, "port"))
				{
					char *cursor = strstr(stdOutResult, "bus=") + 4;
					size_t length = (size_t)(strchr(cursor, ' ') - cursor);
					memcpy(portLocation, cursor, length);
					portLocation[length] = '\0';
					strcat(portLocation, "-");
					cursor = strstr(stdOutResult, "hubaddr=") + 8;
					length = (size_t)(strchr(cursor, ' ') - cursor);
					memcpy(temp, cursor, length);
					temp[length] = '\0';
					strcat(portLocation, temp);
					strcat(portLocation, ".");
					cursor = strstr(stdOutResult, "port=") + 5;
					length = (size_t)(strchr(cursor, ' ') - cursor);
					memcpy(temp, cursor, length);
					temp[length] = '\0';
					strcat(portLocation, temp);
					break;
				}
			pclose(pipe);
		}
		free(temp);
	}
	else
		strcpy(portLocation, "0-0");

	// Parse port VID and PID
	if (deviceInfo)
	{
		char *temp = (char*)malloc(64);
		sprintf(temp, "sysctl -a | grep \"%s\"", deviceInfo);
		pipe = popen(temp, "r");
		if (pipe)
		{
			while (fgets(stdOutResult, bufferSize, pipe))
				if (strstr(stdOutResult, "vendor") && strstr(stdOutResult, "product"))// && strstr(stdOutResult, "sernum"))
				{
					char *cursor = strstr(stdOutResult, "vendor=") + 7;
					size_t length = (size_t)(strchr(cursor, ' ') - cursor);
					memcpy(temp, cursor, length);
					temp[length] = '\0';
					*vendorID = atoi(temp);
					cursor = strstr(stdOutResult, "product=") + 8;
					length = (size_t)(strchr(cursor, ' ') - cursor);
					memcpy(temp, cursor, length);
					temp[length] = '\0';
					*productID = atoi(temp);
					break;
				}
			pclose(pipe);
		}
		free(temp);
	}

	// Clean up memory and return result
	free(stdOutResult);
	if (deviceInfo)
		free(deviceInfo);
	return (deviceLocation ? 1 : 0);
}

void searchForComPorts(serialPortVector* comPorts)
{
	// Open the FreeBSD dev directory
	DIR *directoryIterator = opendir("/dev/");
	if (directoryIterator)
	{
		// Read all files in the current directory
		struct dirent *directoryEntry = readdir(directoryIterator);
		while (directoryEntry)
		{
			// See if the file names a potential serial port
			if ((strlen(directoryEntry->d_name) >= 4) && (directoryEntry->d_name[0] != '.') &&
					(((directoryEntry->d_name[0] == 't') && (directoryEntry->d_name[1] == 't') && (directoryEntry->d_name[2] == 'y') && (directoryEntry->d_name[3] != 'v')) ||
					 ((directoryEntry->d_name[0] == 'c') && (directoryEntry->d_name[1] == 'u') && (directoryEntry->d_name[2] == 'a'))))
			{
				// Ensure that the file is not an init or a lock file
				if ((strlen(directoryEntry->d_name) < 5) ||
						(memcmp(".init", directoryEntry->d_name + strlen(directoryEntry->d_name) - 5, 5) &&
						 memcmp(".lock", directoryEntry->d_name + strlen(directoryEntry->d_name) - 5, 5)))
				{
					// Determine system name of port
					char* systemName = (char*)malloc(256);
					strcpy(systemName, "/dev/");
					strcat(systemName, directoryEntry->d_name);

					// Determine location of port
					int vendorID = -1, productID = -1;
					char* portLocation = (char*)malloc(256);
					char isUSB = getPortDetails(directoryEntry->d_name + 3, portLocation, &vendorID, &productID);

					// Check if port is already enumerated
					serialPort *port = fetchPort(comPorts, systemName);
					if (port)
					{
						// See if device has changed locations
						port->enumerated = 1;
						if (isUSB)
						{
							int oldLength = strlen(port->portLocation);
							int newLength = strlen(portLocation);
							if (oldLength != newLength)
							{
								port->portLocation = (char*)realloc(port->portLocation, newLength + 1);
								strcpy(port->portLocation, portLocation);
							}
							else if (memcmp(port->portLocation, portLocation, newLength))
								strcpy(port->portLocation, portLocation);
						}
					}
					else
					{
						// Set static friendly name
						char* friendlyName = (char*)malloc(256);
						if (directoryEntry->d_name[0] == 'c')
							strcpy(friendlyName, "Serial Port");
						else
							strcpy(friendlyName, "Serial Port (Dial-In)");

						// Add the port to the list if it is not a directory
						struct stat fileStats;
						stat(systemName, &fileStats);
						if (!S_ISDIR(fileStats.st_mode))
							pushBack(comPorts, systemName, friendlyName, friendlyName, portLocation, vendorID, productID);

						// Clean up memory
						free(friendlyName);
					}

					// Clean up memory
					free(portLocation);
					free(systemName);
				}
			}
			directoryEntry = readdir(directoryIterator);
		}

		// Close the directory
		closedir(directoryIterator);
	}
}

baud_rate getBaudRateCode(baud_rate baudRate)
{
	return baudRate;
}

int setBaudRateCustom(int portFD, baud_rate baudRate)
{
	// All baud rates allowed by default on this OS
	return -1;
}

// OpenBSD-specific functionality
#elif defined(__OpenBSD__)

char getUsbPortDetails(const char* usbDeviceFile, char* portLocation, char* friendlyName, char** description, int* vendorID, int* productID)
{
	// Only continue if this is a USB device
	char found = 0;
	sprintf(portLocation, "0-0");
	if (usbDeviceFile[0] != 'U')
		return found;

	// Attempt to locate the device in dmesg
	size_t bufferSize = 1024;
	char *stdOutResult = (char*)malloc(bufferSize), *device = (char*)malloc(64);
	snprintf(stdOutResult, bufferSize, "dmesg | grep ucom%s | tail -1", usbDeviceFile + 1);
	FILE *pipe = popen(stdOutResult, "r");
	device[0] = '\0';
	if (pipe)
	{
		while (fgets(stdOutResult, bufferSize, pipe))
			if (strstr(stdOutResult, " at "))
			{
				found = 1;
				sprintf(friendlyName, "ucom%s", usbDeviceFile + 1);
				strcpy(device, strstr(stdOutResult, " at ") + 4);
				device[strcspn(device, "\r\n")] = '\0';
			}
		pclose(pipe);
	}

	// Parse port location and description
	char *address = NULL, *port = NULL;
	if (device[0])
	{
		char* usbFile = (char*)malloc(64);
		for (int bus = 0; (bus < 255) && (!address || !port); ++bus)
		{
			// Ensure this bus exists
			struct stat fileStats;
			sprintf(usbFile, "/dev/usb%d", bus);
			if (stat(usbFile, &fileStats))
				continue;

			// Read the USB address and description
			snprintf(stdOutResult, bufferSize, "usbdevs -v -d /dev/usb%d 2>/dev/null | grep -B 2 %s", bus, device);
			pipe = popen(stdOutResult, "r");
			if (pipe)
			{
				while (fgets(stdOutResult, bufferSize, pipe))
				{
					if (strstr(stdOutResult, "addr ") && strrchr(stdOutResult, ','))
					{
						char *product = strrchr(stdOutResult, ',') + 2;
						product[strcspn(product, "\r\n")] = '\0';
						*description = (char*)realloc(*description, strlen(product) + 1);
						memcpy(*description, product, strlen(product) + 1);
					}
					if (strstr(stdOutResult, "addr "))
					{
						address = strstr(stdOutResult, "addr ") + 5;
						char *vid = strstr(stdOutResult, ": ") + 2;
						char *pid = strchr(vid, ':');
						*pid = '\0';
						*(strchr(pid+1, ' ')) = '\0';
						while (address[0] == '0')
							address = address + 1;
						*(strchr(address, ':')) = '\0';
						sprintf(portLocation, "%d-%s", bus, address);
						*vendorID = (int)strtol(vid, NULL, 16);
						*productID = (int)strtol(pid + 1, NULL, 16);
					}
				}
				pclose(pipe);
			}

			// Read the USB port location
			snprintf(stdOutResult, bufferSize, "dmesg | grep \"%s at \" | tail -1", device);
			pipe = popen(stdOutResult, "r");
			if (pipe)
			{
				while (fgets(stdOutResult, bufferSize, pipe))
					if (strstr(stdOutResult, "port "))
					{
						port = strstr(stdOutResult, "port ") + 5;
						*(strchr(port, ' ')) = '\0';
						strcat(portLocation, ".");
						strcat(portLocation, port);
					}
				pclose(pipe);
			}
		}
		free(usbFile);
	}

	// Clean up memory and return result
	free(device);
	free(stdOutResult);
	return found;
}

void searchForComPorts(serialPortVector* comPorts)
{
	// Open the FreeBSD dev directory
	DIR *directoryIterator = opendir("/dev/");
	if (directoryIterator)
	{
		// Read all files in the current directory
		struct dirent *directoryEntry = readdir(directoryIterator);
		while (directoryEntry)
		{
			// See if the file names a potential serial port
			if ((strlen(directoryEntry->d_name) >= 4) && (directoryEntry->d_name[0] != '.') &&
					(((directoryEntry->d_name[0] == 't') && (directoryEntry->d_name[1] == 't') && (directoryEntry->d_name[2] == 'y') && (directoryEntry->d_name[3] != 'v')) ||
					 ((directoryEntry->d_name[0] == 'c') && (directoryEntry->d_name[1] == 'u') && (directoryEntry->d_name[2] == 'a')) ||
					 ((directoryEntry->d_name[0] == 'd') && (directoryEntry->d_name[1] == 't') && (directoryEntry->d_name[2] == 'y'))))
			{
				// Determine system name of port
				char* systemName = (char*)malloc(256);
				strcpy(systemName, "/dev/");
				strcat(systemName, directoryEntry->d_name);

				// Set static friendly name and description
				char* friendlyName = (char*)malloc(64);
				char* description = (char*)malloc(32);
				strcpy(friendlyName, "Serial Port");
				strcpy(description, "Serial Port");

				// Determine location and description of port
				int vendorID = -1, productID = -1;
				char* portLocation = (char*)malloc(256);
				char isUSB = getUsbPortDetails(directoryEntry->d_name + 3, portLocation, friendlyName, &description, &vendorID, &productID);

				// Update friendly name
				if ((directoryEntry->d_name[0] != 'c') && (directoryEntry->d_name[0] != 'd'))
					strcat(friendlyName, " (Dial-In)");

				// Check if port is already enumerated
				serialPort *port = fetchPort(comPorts, systemName);
				if (port)
				{
					// See if device has changed locations
					port->enumerated = 1;
					if (isUSB)
					{
						int oldLength = strlen(port->portLocation);
						int newLength = strlen(portLocation);
						if (oldLength != newLength)
						{
							port->portLocation = (char*)realloc(port->portLocation, newLength + 1);
							strcpy(port->portLocation, portLocation);
						}
						else if (memcmp(port->portLocation, portLocation, newLength))
							strcpy(port->portLocation, portLocation);
					}
				}
				else
				{
					// Add the port to the list if it is not a directory
					struct stat fileStats;
					stat(systemName, &fileStats);
					if (!S_ISDIR(fileStats.st_mode) && isUSB)
						pushBack(comPorts, systemName, friendlyName, description, portLocation, vendorID, productID);
				}

				// Clean up memory
				free(portLocation);
				free(description);
				free(friendlyName);
				free(systemName);
			}
			directoryEntry = readdir(directoryIterator);
		}

		// Close the directory
		closedir(directoryIterator);
	}
}

baud_rate getBaudRateCode(baud_rate baudRate)
{
	return baudRate;
}

int setBaudRateCustom(int portFD, baud_rate baudRate)
{
	// All baud rates allowed by default on this OS
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
	io_object_t serialPort;
	io_iterator_t serialPortIterator;
	char comPortCu[1024], comPortTty[1024];
	char friendlyName[1024], portLocation[1024];

	// Enumerate serial ports on machine
	IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching(kIOSerialBSDServiceValue), &serialPortIterator);
	while ((serialPort = IOIteratorNext(serialPortIterator)))
	{
		// Get serial port information
		char isUSB = 0;
		friendlyName[0] = '\0';
		int vendorID = -1, productID = -1;
		io_registry_entry_t parent = 0, service = serialPort;
		while (service)
		{
			if (IOObjectConformsTo(service, "IOUSBDevice"))
			{
				IORegistryEntryGetName(service, friendlyName);
				isUSB = 1;
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

		// Get VID, PID, Serial Number, Bus Number, and Port Address
		if (isUSB)
		{
			CFTypeRef propertyRef = IORegistryEntrySearchCFProperty(serialPort, kIOServicePlane, CFSTR("locationID"), kCFAllocatorDefault, kIORegistryIterateRecursively | kIORegistryIterateParents);
			if (propertyRef)
			{
				int locationID = 0;
				char multiHub = 0, tempLocation[64];
				CFNumberGetValue(propertyRef, kCFNumberIntType, &locationID);
				CFRelease(propertyRef);
				snprintf(portLocation, sizeof(portLocation), "%d", (locationID >> 24) & 0x000000FF);
				strcat(portLocation, "-");
				while (locationID & 0x00F00000)
				{
					if (multiHub)
						strcat(portLocation, ".");
					snprintf(tempLocation, sizeof(tempLocation), "%d", (locationID >> 20) & 0x0000000F);
					strcat(portLocation, tempLocation);
					locationID <<= 4;
					multiHub = 1;
				}
			}
			propertyRef = IORegistryEntrySearchCFProperty(serialPort, kIOServicePlane, CFSTR("idVendor"), kCFAllocatorDefault, kIORegistryIterateRecursively | kIORegistryIterateParents);
			if (propertyRef)
			{
				CFNumberGetValue(propertyRef, kCFNumberIntType, &vendorID);
				CFRelease(propertyRef);
			}
			propertyRef = IORegistryEntrySearchCFProperty(serialPort, kIOServicePlane, CFSTR("idProduct"), kCFAllocatorDefault, kIORegistryIterateRecursively | kIORegistryIterateParents);
			if (propertyRef)
			{
				CFNumberGetValue(propertyRef, kCFNumberIntType, &productID);
				CFRelease(propertyRef);
			}
		}
		else
			strcpy(portLocation, "0-0");

		// Check if callout port is already enumerated
		struct serialPort *port = fetchPort(comPorts, comPortCu);
		if (port)
		{
			// See if device has changed locations
			port->enumerated = 1;
			if (isUSB)
			{
				int oldLength = strlen(port->portLocation);
				int newLength = strlen(portLocation);
				if (oldLength != newLength)
				{
					port->portLocation = (char*)realloc(port->portLocation, newLength + 1);
					strcpy(port->portLocation, portLocation);
				}
				else if (memcmp(port->portLocation, portLocation, newLength))
					strcpy(port->portLocation, portLocation);
			}
		}
		else
			pushBack(comPorts, comPortCu, friendlyName, friendlyName, portLocation, vendorID, productID);

		// Check if dialin port is already enumerated
		port = fetchPort(comPorts, comPortTty);
		strcat(friendlyName, " (Dial-In)");
		if (port)
		{
			// See if device has changed locations
			port->enumerated = 1;
			if (isUSB)
			{
				int oldLength = strlen(port->portLocation);
				int newLength = strlen(portLocation);
				if (oldLength != newLength)
				{
					port->portLocation = (char*)realloc(port->portLocation, newLength + 1);
					strcpy(port->portLocation, portLocation);
				}
				else if (memcmp(port->portLocation, portLocation, newLength))
					strcpy(port->portLocation, portLocation);
			}
		}
		else
			pushBack(comPorts, comPortTty, friendlyName, friendlyName, portLocation, vendorID, productID);
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

int verifyAndSetUserPortGroup(const char *portFile)
{
	// Check if the user can currently access the port file
	int numGroups = getgroups(0, NULL);
	int userCanAccess = (faccessat(0, portFile, R_OK | W_OK, AT_EACCESS) == 0);

	// Attempt to acquire access if not available
	if (!userCanAccess)
	{
		// Ensure that the port still exists
		struct stat fileStats;
		if (stat(portFile, &fileStats) == 0)
		{
			// Check if the user is part of the group that owns the port
			int userPartOfPortGroup = 0;
			gid_t *userGroups = (gid_t*)malloc(numGroups * sizeof(gid_t));
			if (getgroups(numGroups, userGroups) >= 0)
				for (int i = 0; i < numGroups; ++i)
					if (userGroups[i] == fileStats.st_gid)
					{
						userPartOfPortGroup = 1;
						break;
					}

			// Attempt to add the user to the group that owns the port
			char *addUserToGroupCmd = (char*)malloc(256);
			if (!userPartOfPortGroup)
			{
				struct group *portGroup;
				struct passwd *userDetails;
				if ((portGroup = getgrgid(fileStats.st_gid)) && (userDetails = getpwuid(geteuid())))
				{
					snprintf(addUserToGroupCmd, 256, "sudo usermod -a -G %s %s", portGroup->gr_name, userDetails->pw_name);
					userCanAccess = (system(addUserToGroupCmd) == 0);
				}
			}

			// Attempt to enable all read/write port permissions
			snprintf(addUserToGroupCmd, 256, "sudo chmod 666 %s", portFile);
			userCanAccess = (system(addUserToGroupCmd) == 0) || userCanAccess;

			// Clean up memory
			free(addUserToGroupCmd);
			free(userGroups);
		}
	}

	// Return whether the user can currently access the serial port
	return userCanAccess;
}
