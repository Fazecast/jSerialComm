/*
 * SerialPort_Windows.c
 *
 *       Created on:  Feb 25, 2012
 *  Last Updated on:  Dec 17, 2021
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

#ifdef _WIN32
#define WINVER _WIN32_WINNT_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA
#define NTDDI_VERSION NTDDI_VISTA
#define WIN32_LEAN_AND_MEAN
#include <initguid.h>
#include <windows.h>
#include <delayimp.h>
#include <stdlib.h>
#include <string.h>
#include <setupapi.h>
#include <devpkey.h>
#include <devguid.h>
#include "ftdi/ftd2xx.h"
#include "WindowsHelperFunctions.h"

// Cached class, method, and field IDs
jclass serialCommClass;
jmethodID serialCommConstructor;
jfieldID serialPortHandleField;
jfieldID comPortField;
jfieldID friendlyNameField;
jfieldID portDescriptionField;
jfieldID eventListenerRunningField;
jfieldID disableConfigField;
jfieldID isDtrEnabledField;
jfieldID isRtsEnabledField;
jfieldID autoFlushIOBuffersField;
jfieldID portLocationField;
jfieldID baudRateField;
jfieldID dataBitsField;
jfieldID stopBitsField;
jfieldID parityField;
jfieldID flowControlField;
jfieldID sendDeviceQueueSizeField;
jfieldID receiveDeviceQueueSizeField;
jfieldID rs485ModeField;
jfieldID rs485DelayBeforeField;
jfieldID rs485DelayAfterField;
jfieldID xonStartCharField;
jfieldID xoffStopCharField;
jfieldID timeoutModeField;
jfieldID readTimeoutField;
jfieldID writeTimeoutField;
jfieldID eventFlagsField;

// Runtime-loadable DLL functions
typedef int (__stdcall *FT_CreateDeviceInfoListFunction)(LPDWORD);
typedef int (__stdcall *FT_GetDeviceInfoListFunction)(FT_DEVICE_LIST_INFO_NODE*, LPDWORD);
typedef int (__stdcall *FT_GetComPortNumberFunction)(FT_HANDLE, LPLONG);
typedef int (__stdcall *FT_SetLatencyTimerFunction)(FT_HANDLE, UCHAR);
typedef int (__stdcall *FT_OpenFunction)(int, FT_HANDLE*);
typedef int (__stdcall *FT_CloseFunction)(FT_HANDLE);

// List of available serial ports
serialPortVector serialPorts = { NULL, 0, 0 };

JNIEXPORT jobjectArray JNICALL Java_com_fazecast_jSerialComm_SerialPort_getCommPorts(JNIEnv *env, jclass serialComm)
{
	// Reset the enumerated flag on all non-open serial ports
	for (int i = 0; i < serialPorts.length; ++i)
		serialPorts.ports[i]->enumerated = (serialPorts.ports[i]->handle != INVALID_HANDLE_VALUE);

	// Enumerate all serial ports present on the current system
	wchar_t comPort[128];
	HDEVINFO devList = SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
	if (devList != INVALID_HANDLE_VALUE)
	{
		// Iterate through all devices
		DWORD devInterfaceIndex = 0;
		DEVPROPTYPE devInfoPropType;
		SP_DEVINFO_DATA devInfoData;
		devInfoData.cbSize = sizeof(devInfoData);
		while (SetupDiEnumDeviceInfo(devList, devInterfaceIndex++, &devInfoData))
		{
			// Fetch the corresponding COM port for this device
			wchar_t *comPortString = NULL;
			DWORD comPortLength = sizeof(comPort) / sizeof(wchar_t);
			HKEY key = SetupDiOpenDevRegKey(devList, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
			if ((key != INVALID_HANDLE_VALUE) && (RegQueryValueExW(key, L"PortName", NULL, NULL, (BYTE*)comPort, &comPortLength) == ERROR_SUCCESS))
				comPortString = (comPort[0] == L'\\') ? (wcsrchr(comPort, L'\\') + 1) : comPort;
			if (key != INVALID_HANDLE_VALUE)
				RegCloseKey(key);
			if (!comPortString)
				continue;

			// Fetch the friendly name for this device
			DWORD friendlyNameLength = 0;
			wchar_t *friendlyNameString = NULL;
			SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_FriendlyName, &devInfoPropType, NULL, 0, &friendlyNameLength, 0);
			if (!friendlyNameLength)
				SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_FRIENDLYNAME, NULL, NULL, 0, &friendlyNameLength);
			if (friendlyNameLength)
			{
				friendlyNameString = (wchar_t*)malloc(friendlyNameLength);
				if (!SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_FriendlyName, &devInfoPropType, (BYTE*)friendlyNameString, friendlyNameLength, NULL, 0) &&
						!SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_FRIENDLYNAME, NULL, (BYTE*)friendlyNameString, friendlyNameLength, NULL))
				{
					friendlyNameString = (wchar_t*)realloc(friendlyNameString, comPortLength);
					wcscpy(friendlyNameString, comPortString);
				}
			}
			else
			{
				friendlyNameString = (wchar_t*)malloc(comPortLength);
				wcscpy(friendlyNameString, comPortString);
			}

			// Fetch the bus-reported device description
			DWORD portDescriptionLength = 0;
			wchar_t *portDescriptionString = NULL;
			SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &devInfoPropType, NULL, 0, &portDescriptionLength, 0);
			if (portDescriptionLength)
			{
				portDescriptionString = (wchar_t*)malloc(portDescriptionLength);
				if (!SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &devInfoPropType, (BYTE*)portDescriptionString, portDescriptionLength, NULL, 0))
				{
					portDescriptionString = (wchar_t*)realloc(portDescriptionString, friendlyNameLength);
					wcscpy(portDescriptionString, friendlyNameString);
				}
			}
			else
			{
				portDescriptionString = (wchar_t*)malloc(friendlyNameLength);
				wcscpy(portDescriptionString, friendlyNameString);
			}

			// Fetch the physical location for this device
			wchar_t *locationString = NULL;
			DWORD locationLength = 0, busNumber = -1, hubNumber = -1, portNumber = -1;
			if (!SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusNumber, &devInfoPropType, (BYTE*)&busNumber, sizeof(busNumber), NULL, 0) &&
					!SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_BUSNUMBER, NULL, (BYTE*)&busNumber, sizeof(busNumber), NULL))
				busNumber = -1;
			if (!SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_Address, &devInfoPropType, (BYTE*)&portNumber, sizeof(portNumber), NULL, 0) &&
					!SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_ADDRESS, NULL, (BYTE*)&portNumber, sizeof(portNumber), NULL))
				portNumber = -1;
			SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_LocationInfo, &devInfoPropType, NULL, 0, &locationLength, 0);
			if (!locationLength)
				SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_LOCATION_INFORMATION, NULL, NULL, 0, &locationLength);
			if (locationLength)
			{
				locationString = (wchar_t*)malloc(locationLength);
				if (SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_LocationInfo, &devInfoPropType, (BYTE*)locationString, locationLength, NULL, 0) ||
						SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_LOCATION_INFORMATION, NULL, (BYTE*)locationString, locationLength, NULL))
				{
					if (wcsstr(locationString, L"Hub"))
						hubNumber = _wtoi(wcschr(wcsstr(locationString, L"Hub"), L'#') + 1);
					if ((portNumber < 0) && wcsstr(locationString, L"Port"))
					{
						wchar_t *portString = wcschr(wcsstr(locationString, L"Port"), L'#') + 1;
						if (portString)
						{
							wchar_t *end = wcschr(portString, L'.');
							if (end)
								*end = L'\0';
						}
						portNumber = _wtoi(portString);
					}
				}
				free(locationString);
			}
			if (busNumber < 0)
				busNumber = 0;
			if (hubNumber < 0)
				hubNumber = 0;
			if (portNumber < 0)
				portNumber = 0;
			locationString = (wchar_t*)malloc(32*sizeof(wchar_t));
			_snwprintf(locationString, 32, L"%d-%d.%d", busNumber, hubNumber, portNumber);

			// Check if port is already enumerated
			serialPort *port = fetchPort(&serialPorts, comPortString);
			if (port)
			{
				// See if device has changed locations
				port->enumerated = 1;
				int oldLength = wcslen(port->portLocation);
				int newLength = wcslen(locationString);
				if (oldLength != newLength)
				{
					port->portLocation = (wchar_t*)realloc(port->portLocation, (1 + newLength) * sizeof(wchar_t));
					wcscpy(port->portLocation, locationString);
				}
				else if (wcscmp(port->portLocation, locationString))
					wcscpy(port->portLocation, locationString);
			}
			else
				pushBack(&serialPorts, comPortString, friendlyNameString, portDescriptionString, locationString);

			// Clean up memory and reset device info structure
			free(locationString);
			free(portDescriptionString);
			free(friendlyNameString);
			devInfoData.cbSize = sizeof(devInfoData);
		}
		SetupDiDestroyDeviceInfoList(devList);
	}

	// Attempt to locate any FTDI-specified port descriptions
	HINSTANCE ftdiLibInstance = LoadLibrary(TEXT("ftd2xx.dll"));
	if (ftdiLibInstance != NULL)
	{
		FT_CreateDeviceInfoListFunction FT_CreateDeviceInfoList = (FT_CreateDeviceInfoListFunction)GetProcAddress(ftdiLibInstance, "FT_CreateDeviceInfoList");
		FT_GetDeviceInfoListFunction FT_GetDeviceInfoList = (FT_GetDeviceInfoListFunction)GetProcAddress(ftdiLibInstance, "FT_GetDeviceInfoList");
		FT_GetComPortNumberFunction FT_GetComPortNumber = (FT_GetComPortNumberFunction)GetProcAddress(ftdiLibInstance, "FT_GetComPortNumber");
		FT_OpenFunction FT_Open = (FT_OpenFunction)GetProcAddress(ftdiLibInstance, "FT_Open");
		FT_CloseFunction FT_Close = (FT_CloseFunction)GetProcAddress(ftdiLibInstance, "FT_Close");
		FT_SetLatencyTimerFunction FT_SetLatencyTimer = (FT_SetLatencyTimerFunction)GetProcAddress(ftdiLibInstance, "FT_SetLatencyTimer");
		if (FT_CreateDeviceInfoList && FT_GetDeviceInfoList && FT_GetComPortNumber && FT_Open && FT_Close && FT_SetLatencyTimer)
		{
			DWORD numDevs;
			if ((FT_CreateDeviceInfoList(&numDevs) == FT_OK) && (numDevs > 0))
			{
				FT_DEVICE_LIST_INFO_NODE *devInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*numDevs);
				if (FT_GetDeviceInfoList(devInfo, &numDevs) == FT_OK)
				{
					for (int i = 0; i < numDevs; ++i)
					{
						// Determine if the port is currently enumerated and already open
						char isOpen = (devInfo[i].Flags & FT_FLAGS_OPENED) ? 1 : 0;
						if (!isOpen)
							for (int j = 0; j < serialPorts.length; ++j)
								if ((memcmp(serialPorts.ports[j]->serialNumber, devInfo[i].SerialNumber, sizeof(serialPorts.ports[j]->serialNumber)) == 0) && (serialPorts.ports[j]->handle != INVALID_HANDLE_VALUE))
								{
									serialPorts.ports[j]->enumerated = 1;
									isOpen = 1;
									break;
								}

						// Update the port description and latency if not already open
						if (!isOpen)
						{
							LONG comPortNumber = 0;
							if ((FT_Open(i, &devInfo[i].ftHandle) == FT_OK) && (FT_GetComPortNumber(devInfo[i].ftHandle, &comPortNumber) == FT_OK))
							{
								// Reduce latency timer to minimum value of 2
								FT_SetLatencyTimer(devInfo[i].ftHandle, 2);

								// Update port description if COM port is actually connected and present in the port list
								FT_Close(devInfo[i].ftHandle);
								swprintf(comPort, sizeof(comPort) / sizeof(wchar_t), L"COM%ld", comPortNumber);
								for (int j = 0; j < serialPorts.length; ++j)
									if (wcscmp(serialPorts.ports[j]->portPath, comPort) == 0)
									{
										serialPorts.ports[j]->enumerated = 1;
										size_t descLength = 8+strlen(devInfo[i].Description);
										wchar_t *newMemory = (wchar_t*)realloc(serialPorts.ports[j]->portDescription, descLength*sizeof(wchar_t));
										if (newMemory)
										{
											serialPorts.ports[j]->portDescription = newMemory;
											MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, devInfo[i].Description, -1, serialPorts.ports[j]->portDescription, descLength);
										}
										memcpy(serialPorts.ports[j]->serialNumber, devInfo[i].SerialNumber, sizeof(serialPorts.ports[j]->serialNumber));
										break;
									}
							}
						}
					}
				}
				free(devInfo);
			}
		}
		FreeLibrary(ftdiLibInstance);
	}

	// Remove all non-enumerated ports from the serial port listing
	for (int i = 0; i < serialPorts.length; ++i)
		if (!serialPorts.ports[i]->enumerated)
		{
			removePort(&serialPorts, serialPorts.ports[i]);
			i--;
		}

	// Get relevant SerialComm methods and fill in com port array
	jobjectArray arrayObject = (*env)->NewObjectArray(env, serialPorts.length, serialCommClass, 0);
	for (int i = 0; i < serialPorts.length; ++i)
	{
		// Create new SerialComm object containing the enumerated values
		jobject serialCommObject = (*env)->NewObject(env, serialCommClass, serialCommConstructor);
		wcscpy(comPort, L"\\\\.\\");
		wcscat(comPort, serialPorts.ports[i]->portPath);
		(*env)->SetObjectField(env, serialCommObject, comPortField, (*env)->NewString(env, (jchar*)comPort, wcslen(comPort)));
		(*env)->SetObjectField(env, serialCommObject, friendlyNameField, (*env)->NewString(env, (jchar*)serialPorts.ports[i]->friendlyName, wcslen(serialPorts.ports[i]->friendlyName)));
		(*env)->SetObjectField(env, serialCommObject, portDescriptionField, (*env)->NewString(env, (jchar*)serialPorts.ports[i]->portDescription, wcslen(serialPorts.ports[i]->portDescription)));
		(*env)->SetObjectField(env, serialCommObject, portLocationField, (*env)->NewString(env, (jchar*)serialPorts.ports[i]->portLocation, wcslen(serialPorts.ports[i]->portLocation)));

		// Add new SerialComm object to array
		(*env)->SetObjectArrayElement(env, arrayObject, i, serialCommObject);
	}
	return arrayObject;
}

JNIEXPORT void JNICALL Java_com_fazecast_jSerialComm_SerialPort_initializeLibrary(JNIEnv *env, jclass serialComm)
{
	// Cache class and method ID as global references
	serialCommClass = (jclass)(*env)->NewGlobalRef(env, serialComm);
	serialCommConstructor = (*env)->GetMethodID(env, serialCommClass, "<init>", "()V");

	// Cache Java fields as global references
	serialPortHandleField = (*env)->GetFieldID(env, serialCommClass, "portHandle", "J");
	comPortField = (*env)->GetFieldID(env, serialCommClass, "comPort", "Ljava/lang/String;");
	friendlyNameField = (*env)->GetFieldID(env, serialCommClass, "friendlyName", "Ljava/lang/String;");
	portDescriptionField = (*env)->GetFieldID(env, serialCommClass, "portDescription", "Ljava/lang/String;");
	portLocationField = (*env)->GetFieldID(env, serialCommClass, "portLocation", "Ljava/lang/String;");
	eventListenerRunningField = (*env)->GetFieldID(env, serialCommClass, "eventListenerRunning", "Z");
	disableConfigField = (*env)->GetFieldID(env, serialCommClass, "disableConfig", "Z");
	isDtrEnabledField = (*env)->GetFieldID(env, serialCommClass, "isDtrEnabled", "Z");
	isRtsEnabledField = (*env)->GetFieldID(env, serialCommClass, "isRtsEnabled", "Z");
	autoFlushIOBuffersField = (*env)->GetFieldID(env, serialCommClass, "autoFlushIOBuffers", "Z");
	baudRateField = (*env)->GetFieldID(env, serialCommClass, "baudRate", "I");
	dataBitsField = (*env)->GetFieldID(env, serialCommClass, "dataBits", "I");
	stopBitsField = (*env)->GetFieldID(env, serialCommClass, "stopBits", "I");
	parityField = (*env)->GetFieldID(env, serialCommClass, "parity", "I");
	flowControlField = (*env)->GetFieldID(env, serialCommClass, "flowControl", "I");
	sendDeviceQueueSizeField = (*env)->GetFieldID(env, serialCommClass, "sendDeviceQueueSize", "I");
	receiveDeviceQueueSizeField = (*env)->GetFieldID(env, serialCommClass, "receiveDeviceQueueSize", "I");
	rs485ModeField = (*env)->GetFieldID(env, serialCommClass, "rs485Mode", "Z");
	rs485DelayBeforeField = (*env)->GetFieldID(env, serialCommClass, "rs485DelayBefore", "I");
	rs485DelayAfterField = (*env)->GetFieldID(env, serialCommClass, "rs485DelayAfter", "I");
	xonStartCharField = (*env)->GetFieldID(env, serialCommClass, "xonStartChar", "B");
	xoffStopCharField = (*env)->GetFieldID(env, serialCommClass, "xoffStopChar", "B");
	timeoutModeField = (*env)->GetFieldID(env, serialCommClass, "timeoutMode", "I");
	readTimeoutField = (*env)->GetFieldID(env, serialCommClass, "readTimeout", "I");
	writeTimeoutField = (*env)->GetFieldID(env, serialCommClass, "writeTimeout", "I");
	eventFlagsField = (*env)->GetFieldID(env, serialCommClass, "eventFlags", "I");
}

JNIEXPORT void JNICALL Java_com_fazecast_jSerialComm_SerialPort_uninitializeLibrary(JNIEnv *env, jclass serialComm)
{
	// Close all open ports
	for (int i = 0; i < serialPorts.length; ++i)
		if (serialPorts.ports[i]->handle != INVALID_HANDLE_VALUE)
			Java_com_fazecast_jSerialComm_SerialPort_closePortNative(env, serialComm, (jlong)(intptr_t)serialPorts.ports[i]);

	// Delete the cached global reference
	(*env)->DeleteGlobalRef(env, serialCommClass);
}

JNIEXPORT jlong JNICALL Java_com_fazecast_jSerialComm_SerialPort_openPortNative(JNIEnv *env, jobject obj)
{
	// Retrieve the serial port parameter fields
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	const wchar_t *portName = (wchar_t*)(*env)->GetStringChars(env, portNameJString, NULL);
	unsigned char disableAutoConfig = (*env)->GetBooleanField(env, obj, disableConfigField);
	unsigned char autoFlushIOBuffers = (*env)->GetBooleanField(env, obj, autoFlushIOBuffersField);

	// Ensure that the serial port still exists and is not already open
	serialPort *port = fetchPort(&serialPorts, portName);
	if (!port)
	{
		// Create port representation and add to serial port listing
		port = pushBack(&serialPorts, portName, L"User-Specified Port", L"User-Specified Port", L"0-0");
	}
	if (!port || (port->handle != INVALID_HANDLE_VALUE))
	{
		(*env)->ReleaseStringChars(env, portNameJString, (const jchar*)portName);
		return 0;
	}

	// Try to open the serial port with read/write access
	if ((port->handle = CreateFileW(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED, NULL)) != INVALID_HANDLE_VALUE)
	{
		// Configure the port parameters and timeouts
		if (!disableAutoConfig && !Java_com_fazecast_jSerialComm_SerialPort_configPort(env, obj, (jlong)(intptr_t)port))
		{
			// Close the port if there was a problem setting the parameters
			PurgeComm(port->handle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
			CancelIoEx(port->handle, NULL);
			SetCommMask(port->handle, 0);
			CloseHandle(port->handle);
			port->handle = INVALID_HANDLE_VALUE;
		}
		else if (autoFlushIOBuffers)
			Java_com_fazecast_jSerialComm_SerialPort_flushRxTxBuffers(env, obj, (jlong)(intptr_t)port);
	}
	else
	{
		port->errorLineNumber = __LINE__ - 15;
		port->errorNumber = GetLastError();
	}

	// Return a pointer to the serial port data structure
	(*env)->ReleaseStringChars(env, portNameJString, (const jchar*)portName);
	return (port->handle != INVALID_HANDLE_VALUE) ? (jlong)(intptr_t)port : 0;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configPort(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Retrieve port parameters from the Java class
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	DWORD baudRate = (DWORD)(*env)->GetIntField(env, obj, baudRateField);
	BYTE byteSize = (BYTE)(*env)->GetIntField(env, obj, dataBitsField);
	int stopBitsInt = (*env)->GetIntField(env, obj, stopBitsField);
	int parityInt = (*env)->GetIntField(env, obj, parityField);
	int flowControl = (*env)->GetIntField(env, obj, flowControlField);
	int timeoutMode = (*env)->GetIntField(env, obj, timeoutModeField);
	int readTimeout = (*env)->GetIntField(env, obj, readTimeoutField);
	int writeTimeout = (*env)->GetIntField(env, obj, writeTimeoutField);
	int eventsToMonitor = (*env)->GetIntField(env, obj, eventFlagsField);
	char xonStartChar = (*env)->GetByteField(env, obj, xonStartCharField);
	char xoffStopChar = (*env)->GetByteField(env, obj, xoffStopCharField);
	DWORD sendDeviceQueueSize = (DWORD)(*env)->GetIntField(env, obj, sendDeviceQueueSizeField);
	DWORD receiveDeviceQueueSize = (DWORD)(*env)->GetIntField(env, obj, receiveDeviceQueueSizeField);
	BYTE rs485ModeEnabled = (BYTE)(*env)->GetBooleanField(env, obj, rs485ModeField);
	BYTE isDtrEnabled = (*env)->GetBooleanField(env, obj, isDtrEnabledField);
	BYTE isRtsEnabled = (*env)->GetBooleanField(env, obj, isRtsEnabledField);
	BYTE stopBits = (stopBitsInt == com_fazecast_jSerialComm_SerialPort_ONE_STOP_BIT) ? ONESTOPBIT : (stopBitsInt == com_fazecast_jSerialComm_SerialPort_ONE_POINT_FIVE_STOP_BITS) ? ONE5STOPBITS : TWOSTOPBITS;
	BYTE parity = (parityInt == com_fazecast_jSerialComm_SerialPort_NO_PARITY) ? NOPARITY : (parityInt == com_fazecast_jSerialComm_SerialPort_ODD_PARITY) ? ODDPARITY : (parityInt == com_fazecast_jSerialComm_SerialPort_EVEN_PARITY) ? EVENPARITY : (parityInt == com_fazecast_jSerialComm_SerialPort_MARK_PARITY) ? MARKPARITY : SPACEPARITY;
	BOOL isParity = (parityInt == com_fazecast_jSerialComm_SerialPort_NO_PARITY) ? FALSE : TRUE;
	BOOL CTSEnabled = (((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_CTS_ENABLED) > 0) ||
			((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_RTS_ENABLED) > 0));
	BOOL DSREnabled = (((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_DSR_ENABLED) > 0) ||
			((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_DTR_ENABLED) > 0));
	BYTE DTRValue = ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_DTR_ENABLED) > 0) ? DTR_CONTROL_HANDSHAKE : (isDtrEnabled ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE);
	BYTE RTSValue = (rs485ModeEnabled ? RTS_CONTROL_TOGGLE :
			(((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_RTS_ENABLED) > 0) ? RTS_CONTROL_HANDSHAKE : (isRtsEnabled ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE)));
	BOOL XonXoffInEnabled = ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_XONXOFF_IN_ENABLED) > 0);
	BOOL XonXoffOutEnabled = ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_XONXOFF_OUT_ENABLED) > 0);

	// Retrieve existing port configuration
	DCB dcbSerialParams;
	memset(&dcbSerialParams, 0, sizeof(DCB));
	dcbSerialParams.DCBlength = sizeof(DCB);
	if (!SetupComm(port->handle, receiveDeviceQueueSize, sendDeviceQueueSize) || !GetCommState(port->handle, &dcbSerialParams))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
		return JNI_FALSE;
	}

	// Set updated port parameters
	dcbSerialParams.BaudRate = baudRate;
	dcbSerialParams.ByteSize = byteSize;
	dcbSerialParams.StopBits = stopBits;
	dcbSerialParams.Parity = parity;
	dcbSerialParams.fParity = isParity;
	dcbSerialParams.fBinary = TRUE;
	dcbSerialParams.fAbortOnError = FALSE;
	dcbSerialParams.fRtsControl = RTSValue;
	dcbSerialParams.fOutxCtsFlow = CTSEnabled;
	dcbSerialParams.fOutxDsrFlow = DSREnabled;
	dcbSerialParams.fDtrControl = DTRValue;
	dcbSerialParams.fDsrSensitivity = DSREnabled;
	dcbSerialParams.fOutX = XonXoffOutEnabled;
	dcbSerialParams.fInX = XonXoffInEnabled;
	dcbSerialParams.fTXContinueOnXoff = TRUE;
	dcbSerialParams.fErrorChar = FALSE;
	dcbSerialParams.fNull = FALSE;
	dcbSerialParams.XonLim = 2048;
	dcbSerialParams.XoffLim = 512;
	dcbSerialParams.XonChar = xonStartChar;
	dcbSerialParams.XoffChar = xoffStopChar;

	// Apply changes
	if (!SetCommState(port->handle, &dcbSerialParams))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
		return JNI_FALSE;
	}
	return Java_com_fazecast_jSerialComm_SerialPort_configTimeouts(env, obj, serialPortPointer, timeoutMode, readTimeout, writeTimeout, eventsToMonitor);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configTimeouts(JNIEnv *env, jobject obj, jlong serialPortPointer, jint timeoutMode, jint readTimeout, jint writeTimeout, jint eventsToMonitor)
{
	// Get event flags from the Java class
	int eventFlags = EV_ERR;
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	if ((eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE) || (eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED))
		eventFlags |= EV_RXCHAR;
	if (eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_WRITTEN)
		eventFlags |= EV_TXEMPTY;

	// Set updated port timeouts
	COMMTIMEOUTS timeouts;
	memset(&timeouts, 0, sizeof(COMMTIMEOUTS));
	timeouts.WriteTotalTimeoutMultiplier = 0;
	if (eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED)
	{
		// Force specific read timeouts if we are monitoring data received
		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
		timeouts.ReadTotalTimeoutConstant = 1000;
		timeouts.WriteTotalTimeoutConstant = 0;
	}
	else if (timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_SCANNER)
	{
		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
		timeouts.ReadTotalTimeoutConstant = 0x0FFFFFFF;
		timeouts.WriteTotalTimeoutConstant = writeTimeout;
	}
	else if (timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_SEMI_BLOCKING)
	{
		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
		timeouts.ReadTotalTimeoutConstant = readTimeout ? readTimeout : 0x0FFFFFFF;
		timeouts.WriteTotalTimeoutConstant = writeTimeout;
	}
	else if (timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING)
	{
		timeouts.ReadIntervalTimeout = 0;
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.ReadTotalTimeoutConstant = readTimeout;
		timeouts.WriteTotalTimeoutConstant = writeTimeout;
	}
	else		// Non-blocking
	{
		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.ReadTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutConstant = writeTimeout;
	}

	// Apply changes
	if (!SetCommTimeouts(port->handle, &timeouts) || !SetCommMask(port->handle, eventFlags))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_flushRxTxBuffers(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	if (PurgeComm(port->handle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR) == 0)
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_waitForEvent(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Create an asynchronous event structure
	OVERLAPPED overlappedStruct;
	memset(&overlappedStruct, 0, sizeof(OVERLAPPED));
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	overlappedStruct.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	jint event = com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_TIMED_OUT;
	if (!overlappedStruct.hEvent)
	{
		port->errorNumber = GetLastError();
		port->errorLineNumber = __LINE__ - 5;
		return event;
	}

	// Wait for a serial port event
	DWORD eventMask = 0, errorMask = 0, waitValue, numBytesTransferred;
	if (!WaitCommEvent(port->handle, &eventMask, &overlappedStruct))
	{
		if ((GetLastError() == ERROR_IO_PENDING) || (GetLastError() == ERROR_INVALID_PARAMETER))
		{
			do { waitValue = WaitForSingleObject(overlappedStruct.hEvent, 500); }
			while ((waitValue == WAIT_TIMEOUT) && port->eventListenerRunning);
			if ((waitValue != WAIT_OBJECT_0) || !GetOverlappedResult(port->handle, &overlappedStruct, &numBytesTransferred, FALSE))
			{
				port->errorNumber = GetLastError();
				port->errorLineNumber = __LINE__ - 3;
				CloseHandle(overlappedStruct.hEvent);
				return event;
			}
		}
		else		// Problem occurred
		{
			port->errorNumber = GetLastError();
			port->errorLineNumber = __LINE__ - 16;
			CloseHandle(overlappedStruct.hEvent);
			return event;
		}
	}

	// Retrieve and clear any serial port errors
	COMSTAT commInfo;
	if (ClearCommError(port->handle, &errorMask, &commInfo))
	{
		if (errorMask & CE_BREAK)
			event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_BREAK_INTERRUPT;
		if (errorMask & CE_FRAME)
			event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_FRAMING_ERROR;
		if (errorMask & CE_OVERRUN)
			event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_FIRMWARE_OVERRUN_ERROR;
		if (errorMask & CE_RXOVER)
			event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_SOFTWARE_OVERRUN_ERROR;
		if (errorMask & CE_RXPARITY)
			event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PARITY_ERROR;
	}

	// Parse any received serial port events
	DWORD modemStatus;
	if (eventMask & EV_BREAK)
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_BREAK_INTERRUPT;
	if (eventMask & EV_TXEMPTY)
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_WRITTEN;
	if ((eventMask & EV_RXCHAR) && (commInfo.cbInQue > 0))
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE;
	if ((eventMask & EV_CTS) && GetCommModemStatus(port->handle, &modemStatus) && (modemStatus & MS_CTS_ON))
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_CTS;
	if ((eventMask & EV_DSR) && GetCommModemStatus(port->handle, &modemStatus) && (modemStatus & MS_DSR_ON))
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DSR;
	if ((eventMask & EV_RING) && GetCommModemStatus(port->handle, &modemStatus) && (modemStatus & MS_RING_ON))
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_RING_INDICATOR;
	if ((eventMask & EV_RLSD) && GetCommModemStatus(port->handle, &modemStatus) && (modemStatus & MS_RLSD_ON))
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_CARRIER_DETECT;

	// Return the serial event type
	CloseHandle(overlappedStruct.hEvent);
	return event;
}

JNIEXPORT jlong JNICALL Java_com_fazecast_jSerialComm_SerialPort_closePortNative(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Force the port to enter non-blocking mode to ensure that any current reads return
	COMMTIMEOUTS timeouts;
	memset(&timeouts, 0, sizeof(COMMTIMEOUTS));
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.ReadIntervalTimeout = MAXDWORD;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 0;
	timeouts.WriteTotalTimeoutConstant = 0;
	SetCommTimeouts(port->handle, &timeouts);

	// Purge any outstanding port operations
	PurgeComm(port->handle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
	CancelIoEx(port->handle, NULL);
	FlushFileBuffers(port->handle);
	SetCommMask(port->handle, 0);

	// Close the port
	port->eventListenerRunning = 0;
	if (!CloseHandle(port->handle))
	{
		port->handle = INVALID_HANDLE_VALUE;
		port->errorLineNumber = __LINE__ - 3;
		port->errorNumber = GetLastError();
		return 0;
	}
	port->handle = INVALID_HANDLE_VALUE;
	return -1;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAvailable(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Retrieve bytes available to read
	COMSTAT commInfo;
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	if (ClearCommError(port->handle, NULL, &commInfo))
		return commInfo.cbInQue;
	else
	{
		port->errorLineNumber = __LINE__ - 4;
		port->errorNumber = GetLastError();
	}
	return -1;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAwaitingWrite(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Retrieve bytes awaiting write
	COMSTAT commInfo;
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	if (ClearCommError(port->handle, NULL, &commInfo))
		return commInfo.cbOutQue;
	else
	{
		port->errorLineNumber = __LINE__ - 4;
		port->errorNumber = GetLastError();
	}
	return -1;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_readBytes(JNIEnv *env, jobject obj, jlong serialPortPointer, jbyteArray buffer, jlong bytesToRead, jlong offset, jint timeoutMode, jint readTimeout)
{
	// Ensure that the allocated read buffer is large enough
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	if (bytesToRead > port->readBufferLength)
	{
		port->errorLineNumber = __LINE__ + 1;
		char *newMemory = (char*)realloc(port->readBuffer, bytesToRead);
		if (!newMemory)
		{
			port->errorNumber = errno;
			return -1;
		}
		port->readBuffer = newMemory;
		port->readBufferLength = bytesToRead;
	}

	// Create an asynchronous result structure
	OVERLAPPED overlappedStruct;
	memset(&overlappedStruct, 0, sizeof(OVERLAPPED));
	overlappedStruct.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (overlappedStruct.hEvent == NULL)
	{
		port->errorNumber = GetLastError();
		port->errorLineNumber = __LINE__ - 4;
		CloseHandle(overlappedStruct.hEvent);
		return -1;
	}

	// Read from the serial port
	BOOL result;
	DWORD numBytesRead = 0;
	if (((result = ReadFile(port->handle, port->readBuffer, bytesToRead, NULL, &overlappedStruct)) == FALSE) && (GetLastError() != ERROR_IO_PENDING))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
	}
	else if ((result = GetOverlappedResult(port->handle, &overlappedStruct, &numBytesRead, TRUE)) == FALSE)
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
	}

	// Return number of bytes read
	CloseHandle(overlappedStruct.hEvent);
	(*env)->SetByteArrayRegion(env, buffer, offset, numBytesRead, (jbyte*)port->readBuffer);
	return (result == TRUE) ? numBytesRead : -1;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_writeBytes(JNIEnv *env, jobject obj, jlong serialPortPointer, jbyteArray buffer, jlong bytesToWrite, jlong offset, jint timeoutMode)
{
	// Create an asynchronous result structure
	OVERLAPPED overlappedStruct;
	memset(&overlappedStruct, 0, sizeof(OVERLAPPED));
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	overlappedStruct.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (overlappedStruct.hEvent == NULL)
	{
		port->errorNumber = GetLastError();
		port->errorLineNumber = __LINE__ - 4;
		CloseHandle(overlappedStruct.hEvent);
		return -1;
	}

	// Write to the serial port
	BOOL result;
	DWORD numBytesWritten = 0;
	jbyte *writeBuffer = (*env)->GetByteArrayElements(env, buffer, 0);
	if (((result = WriteFile(port->handle, writeBuffer+offset, bytesToWrite, NULL, &overlappedStruct)) == FALSE) && (GetLastError() != ERROR_IO_PENDING))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
	}
	else if ((result = GetOverlappedResult(port->handle, &overlappedStruct, &numBytesWritten, TRUE)) == FALSE)
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
	}

	// Return number of bytes written
	CloseHandle(overlappedStruct.hEvent);
	(*env)->ReleaseByteArrayElements(env, buffer, writeBuffer, JNI_ABORT);
	return (result == TRUE) ? numBytesWritten : -1;
}

JNIEXPORT void JNICALL Java_com_fazecast_jSerialComm_SerialPort_setEventListeningStatus(JNIEnv *env, jobject obj, jlong serialPortPointer, jboolean eventListenerRunning)
{
	((serialPort*)(intptr_t)serialPortPointer)->eventListenerRunning = eventListenerRunning;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_setBreak(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	if (!SetCommBreak(port->handle))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_clearBreak(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	if (!ClearCommBreak(port->handle))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_setRTS(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	if (!EscapeCommFunction(port->handle, SETRTS))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_clearRTS(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	if (!EscapeCommFunction(port->handle, CLRRTS))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_presetRTS(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	const char *portName = (*env)->GetStringUTFChars(env, portNameJString, NULL);
	const char* comPort = strrchr(portName, '\\');

	// Try to preset the RTS mode of the COM port using a Windows command
	int result = 0;
	if (comPort != NULL)
	{
		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		char commandString[64];
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		si.wShowWindow = SW_HIDE;
		sprintf(commandString, "mode.com %s rts=on", comPort + 1);
		result = CreateProcess(NULL, commandString, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
	return (result != 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_preclearRTS(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	const char *portName = (*env)->GetStringUTFChars(env, portNameJString, NULL);
	const char* comPort = strrchr(portName, '\\');

	// Try to preset the RTS mode of the COM port using a Windows command
	int result = 0;
	if (comPort != NULL)
	{
		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		char commandString[64];
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		si.wShowWindow = SW_HIDE;
		sprintf(commandString, "mode.com %s rts=off", comPort + 1);
		result = CreateProcess(NULL, commandString, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
	return (result != 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_setDTR(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	if (!EscapeCommFunction(port->handle, SETDTR))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_clearDTR(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	if (!EscapeCommFunction(port->handle, CLRDTR))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = GetLastError();
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_presetDTR(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	const char *portName = (*env)->GetStringUTFChars(env, portNameJString, NULL);
	const char* comPort = strrchr(portName, '\\');

	// Try to preset the DTR mode of the COM port using a Windows command
	int result = 0;
	if (comPort != NULL)
	{
		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		char commandString[64];
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		si.wShowWindow = SW_HIDE;
		sprintf(commandString, "mode.com %s dtr=on", comPort + 1);
		result = CreateProcess(NULL, commandString, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
	return (result != 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_preclearDTR(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	const char *portName = (*env)->GetStringUTFChars(env, portNameJString, NULL);
	const char* comPort = strrchr(portName, '\\');

	// Try to preset the DTR mode of the COM port using a Windows command
	int result = 0;
	if (comPort != NULL)
	{
		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		char commandString[64];
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		si.wShowWindow = SW_HIDE;
		sprintf(commandString, "mode.com %s dtr=off", comPort + 1);
		result = CreateProcess(NULL, commandString, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
	return (result != 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getCTS(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	DWORD modemStatus = 0;
	return GetCommModemStatus(((serialPort*)(intptr_t)serialPortPointer)->handle, &modemStatus) && (modemStatus & MS_CTS_ON);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getDSR(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	DWORD modemStatus = 0;
	return GetCommModemStatus(((serialPort*)(intptr_t)serialPortPointer)->handle, &modemStatus) && (modemStatus & MS_DSR_ON);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getDCD(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	DWORD modemStatus = 0;
	return GetCommModemStatus(((serialPort*)(intptr_t)serialPortPointer)->handle, &modemStatus) && (modemStatus & MS_RLSD_ON);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getDTR(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	return (*env)->GetBooleanField(env, obj, isDtrEnabledField);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getRTS(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	return (*env)->GetBooleanField(env, obj, isRtsEnabledField);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getRI(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	DWORD modemStatus = 0;
	return GetCommModemStatus(((serialPort*)(intptr_t)serialPortPointer)->handle, &modemStatus) && (modemStatus & MS_RING_ON);
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_getLastErrorLocation(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	return ((serialPort*)(intptr_t)serialPortPointer)->errorLineNumber;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_getLastErrorCode(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	return ((serialPort*)(intptr_t)serialPortPointer)->errorNumber;
}

#endif
