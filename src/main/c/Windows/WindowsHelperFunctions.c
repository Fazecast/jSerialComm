/*
 * WindowsHelperFunctions.c
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

#ifdef _WIN32
#define WINVER _WIN32_WINNT_WINXP
#define _WIN32_WINNT _WIN32_WINNT_WINXP
#define NTDDI_VERSION NTDDI_WINXP
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "WindowsHelperFunctions.h"

// Common storage functionality
serialPort* pushBack(serialPortVector* vector, const wchar_t* key, const wchar_t* friendlyName, const wchar_t* description, const wchar_t* location, int vid, int pid)
{
	// Allocate memory for the new SerialPort storage structure
	unsigned char containsSlashes = ((key[0] == L'\\') && (key[1] == L'\\') && (key[2] == L'.') && (key[3] == L'\\'));
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
	port->vendorID = vid;
	port->productID = pid;
	port->portPath = (wchar_t*)malloc((wcslen(key)+(containsSlashes ? 1 : 5))*sizeof(wchar_t));
	port->portLocation = (wchar_t*)malloc((wcslen(location)+1)*sizeof(wchar_t));
	port->friendlyName = (wchar_t*)malloc((wcslen(friendlyName)+1)*sizeof(wchar_t));
	port->portDescription = (wchar_t*)malloc((wcslen(description)+1)*sizeof(wchar_t));
	if (!port->portPath || !port->portLocation || !port->friendlyName || !port->portDescription)
	{
		// Clean up memory associated with the port
		vector->length--;
		if (port->portPath)
			free(port->portPath);
		if (port->portLocation)
			free(port->portLocation);
		if (port->friendlyName)
			free(port->friendlyName);
		if (port->portDescription)
			free(port->portDescription);
		free(port);
		return NULL;
	}

	// Store port strings
	if (containsSlashes)
		wcscpy_s(port->portPath, wcslen(key)+1, key);
	else
	{
		wcscpy_s(port->portPath, wcslen(key)+5, L"\\\\.\\");
		wcscat_s(port->portPath, wcslen(key)+5, key);
	}
	wcscpy_s(port->portLocation, wcslen(location)+1, location);
	wcscpy_s(port->friendlyName, wcslen(friendlyName)+1, friendlyName);
	wcscpy_s(port->portDescription, wcslen(description)+1, description);

	// Return the newly created serial port structure
	return port;
}

serialPort* fetchPort(serialPortVector* vector, const wchar_t* key)
{
	// Retrieve the serial port specified by the passed-in key
	int keyOffset = ((key[0] == L'\\') && (key[1] == L'\\') && (key[2] == L'.') && (key[3] == L'\\')) ? 0 : 4;
	for (int i = 0; i < vector->length; ++i)
		if (wcscmp(key, vector->ports[i]->portPath + keyOffset) == 0)
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

void cleanUpVector(serialPortVector* vector)
{
	while (vector->length)
		removePort(vector, vector->ports[0]);
	if (vector->ports)
		free(vector->ports);
	vector->ports = NULL;
	vector->length = vector->capacity = 0;
}

// Windows-specific functionality
void reduceLatencyToMinimum(const wchar_t* portName, unsigned char requestElevatedPermissions)
{
	// Search for this port in all FTDI enumerated ports
	HKEY key, paramKey;
	DWORD maxSubkeySize, maxPortNameSize = 8;
	if ((RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Enum\\FTDIBUS", 0, KEY_READ, &key) == ERROR_SUCCESS) &&
			(RegQueryInfoKeyW(key, NULL, NULL, NULL, NULL, &maxSubkeySize, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS))
	{
		maxSubkeySize += 32;
		DWORD index = 0, subkeySize = maxSubkeySize;
		wchar_t *subkey = (wchar_t*)malloc(maxSubkeySize * sizeof(wchar_t)), *portPath = (wchar_t*)malloc(maxPortNameSize * sizeof(wchar_t));
		while (subkey && portPath && (RegEnumKeyExW(key, index++, subkey, &subkeySize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS))
		{
			// Retrieve the current port latency value
			size_t convertedSize;
			char *subkeyString = NULL;
			subkeySize = maxSubkeySize;
			DWORD desiredLatency = 2, oldLatency = 2;
			if ((wcstombs_s(&convertedSize, NULL, 0, subkey, 0) == 0) && (convertedSize < 255))
			{
				subkeyString = (char*)malloc(convertedSize);
				if (subkeyString && (wcstombs_s(NULL, subkeyString, convertedSize, subkey, convertedSize - 1) == 0) &&
						(wcscat_s(subkey, maxSubkeySize, L"\\0000\\Device Parameters") == 0))
				{
					if (RegOpenKeyExW(key, subkey, 0, KEY_QUERY_VALUE, &paramKey) == ERROR_SUCCESS)
					{
						DWORD oldLatencySize = sizeof(DWORD), portNameSize = maxPortNameSize * sizeof(wchar_t);
						if ((RegQueryValueExW(paramKey, L"PortName", NULL, NULL, (LPBYTE)portPath, &portNameSize) == ERROR_SUCCESS) && (wcscmp(portName, portPath) == 0))
							RegQueryValueExW(paramKey, L"LatencyTimer", NULL, NULL, (LPBYTE)&oldLatency, &oldLatencySize);
						RegCloseKey(paramKey);
					}
				}
			}

			// Update the port latency value if it is too high
			if (oldLatency > desiredLatency)
			{
				if (RegOpenKeyExW(key, subkey, 0, KEY_SET_VALUE, &paramKey) == ERROR_SUCCESS)
				{
					RegSetValueExW(paramKey, L"LatencyTimer", 0, REG_DWORD, (LPBYTE)&desiredLatency, sizeof(desiredLatency));
					RegCloseKey(paramKey);
				}
				else if (requestElevatedPermissions)
				{
					// Create registry update file
					char *workingDirectory = _getcwd(NULL, 0);
					wchar_t *workingDirectoryWide = _wgetcwd(NULL, 0);
					int workingDirectoryLength = strlen(workingDirectory) + 1;
					char *registryFileName = (char*)malloc(workingDirectoryLength + 8);
					wchar_t *paramsString = (wchar_t*)malloc((workingDirectoryLength + 11) * sizeof(wchar_t));
					sprintf(registryFileName, "%s\\del.reg", workingDirectory);
					swprintf(paramsString, workingDirectoryLength + 11, L"/s %s\\del.reg", workingDirectoryWide);
					FILE *registryFile = fopen(registryFileName, "wb");
					if (registryFile)
					{
						fprintf(registryFile, "Windows Registry Editor Version 5.00\n\n");
						fprintf(registryFile, "[HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Enum\\FTDIBUS\\%s\\0000\\Device Parameters]\n", subkeyString);
						fprintf(registryFile, "\"LatencyTimer\"=dword:00000002\n\n");
						fclose(registryFile);
					}

					// Launch a new administrative process to update the registry value
					SHELLEXECUTEINFOW shExInfo = { 0 };
					shExInfo.cbSize = sizeof(shExInfo);
					shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
					shExInfo.hwnd = NULL;
					shExInfo.lpVerb = L"runas";
					shExInfo.lpFile = L"C:\\Windows\\regedit.exe";
					shExInfo.lpParameters = paramsString;
					shExInfo.lpDirectory = NULL;
					shExInfo.nShow = SW_SHOW;
					shExInfo.hInstApp = 0;
					if (ShellExecuteExW(&shExInfo))
					{
						WaitForSingleObject(shExInfo.hProcess, INFINITE);
						CloseHandle(shExInfo.hProcess);
					}

					// Delete the registry update file
					remove(registryFileName);
					free(workingDirectoryWide);
					free(workingDirectory);
					free(registryFileName);
					free(paramsString);
				}
			}

			// Clean up memory
			if (subkeyString)
				free(subkeyString);
		}
		RegCloseKey(key);
		free(portPath);
		free(subkey);
	}
}

int getPortPathFromSerial(wchar_t* portPath, int portPathLength, const char* serialNumber)
{
	// Search for this port in all FTDI enumerated ports
	int found = 0;
	HKEY key, paramKey;
	DWORD maxSubkeySize;
	if ((RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Enum\\FTDIBUS", 0, KEY_READ, &key) == ERROR_SUCCESS) &&
			(RegQueryInfoKeyW(key, NULL, NULL, NULL, NULL, &maxSubkeySize, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS))
	{
		maxSubkeySize += 32;
		DWORD index = 0, subkeySize = maxSubkeySize;
		wchar_t *subkey = (wchar_t*)malloc(maxSubkeySize * sizeof(wchar_t));
		while (subkey && (RegEnumKeyExW(key, index++, subkey, &subkeySize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS))
		{
			// Convert this string from wchar* to char*
			size_t convertedSize;
			subkeySize = maxSubkeySize;
			if ((wcstombs_s(&convertedSize, NULL, 0, subkey, 0) == 0) && (convertedSize < 255))
			{
				char *subkeyString = (char*)malloc(convertedSize);
				if (subkeyString && (wcstombs_s(NULL, subkeyString, convertedSize, subkey, convertedSize - 1) == 0))
				{
					// Determine if this device matches the specified serial number
					if (serialNumber && strstr(subkeyString, serialNumber) && (wcscat_s(subkey, maxSubkeySize, L"\\0000\\Device Parameters") == 0))
					{
						DWORD portNameSize = portPathLength;
						if ((RegOpenKeyExW(key, subkey, 0, KEY_QUERY_VALUE, &paramKey) == ERROR_SUCCESS) &&
								(RegQueryValueExW(paramKey, L"PortName", NULL, NULL, (LPBYTE)portPath, &portNameSize) == ERROR_SUCCESS))
						{
							found = 1;
							RegCloseKey(paramKey);
						}
					}
				}
				if (subkeyString)
					free(subkeyString);
			}
		}
		RegCloseKey(key);
		if (subkey)
			free(subkey);
	}
	return found;
}

#endif
