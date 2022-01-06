/*
 * WindowsHelperFunctions.c
 *
 *       Created on:  May 05, 2015
 *  Last Updated on:  Jan 06, 2022
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

#ifdef _WIN32
#include <stdlib.h>
#include <string.h>
#include "WindowsHelperFunctions.h"

// Common storage functionality
serialPort* pushBack(serialPortVector* vector, const wchar_t* key, const wchar_t* friendlyName, const wchar_t* description, const wchar_t* location)
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
	port->handle = (void*)-1;
	port->enumerated = 1;
	port->portPath = (wchar_t*)malloc((wcslen(key)+1)*sizeof(wchar_t));
	port->portLocation = (wchar_t*)malloc((wcslen(location)+1)*sizeof(wchar_t));
	port->friendlyName = (wchar_t*)malloc((wcslen(friendlyName)+1)*sizeof(wchar_t));
	port->portDescription = (wchar_t*)malloc((wcslen(description)+1)*sizeof(wchar_t));

	// Store port strings
	wcscpy_s(port->portPath, wcslen(key)+1, key);
	wcscpy_s(port->portLocation, wcslen(location)+1, location);
	wcscpy_s(port->friendlyName, wcslen(friendlyName)+1, friendlyName);
	wcscpy_s(port->portDescription, wcslen(description)+1, description);

	// Return the newly created serial port structure
	return port;
}

serialPort* fetchPort(serialPortVector* vector, const wchar_t* key)
{
	for (int i = 0; i < vector->length; ++i)
		if (wcscmp(key, vector->ports[i]->portPath) == 0)
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

#endif
