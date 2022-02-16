/*
 * WindowsHelperFunctions.c
 *
 *       Created on:  May 05, 2015
 *  Last Updated on:  Feb 16, 2022
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
#define WINVER _WIN32_WINNT_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA
#define NTDDI_VERSION NTDDI_VISTA
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>
#include "WindowsHelperFunctions.h"

// Common storage functionality
serialPort* pushBack(serialPortVector* vector, const wchar_t* key, const wchar_t* friendlyName, const wchar_t* description, const wchar_t* location)
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
	port->portPath = (wchar_t*)malloc((wcslen(key)+(containsSlashes ? 1 : 5))*sizeof(wchar_t));
	port->portLocation = (wchar_t*)malloc((wcslen(location)+1)*sizeof(wchar_t));
	port->friendlyName = (wchar_t*)malloc((wcslen(friendlyName)+1)*sizeof(wchar_t));
	port->portDescription = (wchar_t*)malloc((wcslen(description)+1)*sizeof(wchar_t));

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

// Windows-specific functionality
void reduceLatencyToMinimum(const wchar_t* portName, unsigned char requestElevatedPermissions)
{
	// Search for this port in all FTDI enumerated ports
	HKEY key, paramKey = 0;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Enum\\FTDIBUS", 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		DWORD index = 0, subkeySize = 255, portNameSize = 16;
		wchar_t *subkey = (wchar_t*)malloc(subkeySize*sizeof(wchar_t)), *regPortName = (wchar_t*)malloc(portNameSize*sizeof(wchar_t));
		while (RegEnumKeyExW(key, index++, subkey, &subkeySize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			DWORD latency = 2, oldLatency = 2, oldLatencySize = sizeof(DWORD);
			char *subkeyString = (char*)malloc(subkeySize + 2);
			memset(subkeyString, 0, subkeySize + 2);
			wcstombs(subkeyString, subkey, subkeySize + 1);
			subkeySize = 255;
			portNameSize = 16;
			memset(regPortName, 0, portNameSize * sizeof(wchar_t));
			wcscat_s(subkey, subkeySize, L"\\0000\\Device Parameters");
			if (RegOpenKeyExW(key, subkey, 0, KEY_QUERY_VALUE, &paramKey) == ERROR_SUCCESS)
			{
				if ((RegQueryValueExW(paramKey, L"PortName", NULL, NULL, (LPBYTE)regPortName, &portNameSize) == ERROR_SUCCESS) && (wcscmp(portName, regPortName) == 0))
					RegQueryValueExW(paramKey, L"LatencyTimer", NULL, NULL, (LPBYTE)&oldLatency, &oldLatencySize);
				RegCloseKey(paramKey);
			}
			if (oldLatency > latency)
			{
				if (RegOpenKeyExW(key, subkey, 0, KEY_SET_VALUE, &paramKey) == ERROR_SUCCESS)
				{
					RegSetValueExW(paramKey, L"LatencyTimer", 0, REG_DWORD, (LPBYTE)&latency, sizeof(latency));
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
			free(subkeyString);
		}
		RegCloseKey(key);
		free(regPortName);
		free(subkey);
	}
}

int getPortPathFromSerial(wchar_t* portPath, const char* serialNumber)
{
	// Search for this port in all FTDI enumerated ports
	int found = 0;
	HKEY key, paramKey = 0;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Enum\\FTDIBUS", 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		DWORD index = 0, subkeySize = 255, portNameSize = 16;
		wchar_t *subkey = (wchar_t*)malloc(subkeySize * sizeof(wchar_t));
		while (RegEnumKeyExW(key, index++, subkey, &subkeySize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			// Determine if this device matches the specified serial number
			char *subkeyString = (char*)malloc(subkeySize + 2);
			memset(subkeyString, 0, subkeySize + 2);
			wcstombs(subkeyString, subkey, subkeySize + 1);
			subkeySize = 255;
			if (strstr(subkeyString, serialNumber))
			{
				portNameSize = 16;
				memset(portPath, 0, portNameSize * sizeof(wchar_t));
				wcscat_s(subkey, subkeySize, L"\\0000\\Device Parameters");
				if (RegOpenKeyExW(key, subkey, 0, KEY_QUERY_VALUE, &paramKey) == ERROR_SUCCESS)
				{
					if (RegQueryValueExW(paramKey, L"PortName", NULL, NULL, (LPBYTE)portPath, &portNameSize) == ERROR_SUCCESS)
						found = 1;
					RegCloseKey(paramKey);
				}
			}
			free(subkeyString);
		}
		RegCloseKey(key);
		free(subkey);
	}
	return found;
}

#endif
