/*
 * SerialPort_Windows.c
 *
 *       Created on:  Feb 25, 2012
 *  Last Updated on:  Dec 02, 2022
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
#include <initguid.h>
#include <windows.h>
#include <delayimp.h>
#include <direct.h>
#include <ntddmodm.h>
#include <ntddser.h>
#include <stdlib.h>
#include <string.h>
#include <setupapi.h>
#include <devpkey.h>
#include <devguid.h>
#include "ftdi/ftd2xx.h"
#include "WindowsHelperFunctions.h"

// Cached class, method, and field IDs
jclass jniErrorClass;
jmethodID serialCommConstructor;
jfieldID serialPortHandleField;
jfieldID comPortField;
jfieldID friendlyNameField;
jfieldID portDescriptionField;
jfieldID vendorIdField;
jfieldID productIdField;
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
jfieldID requestElevatedPermissionsField;
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
typedef BOOL (__stdcall *SetupDiGetDevicePropertyWFunction)(HDEVINFO, PSP_DEVINFO_DATA, const DEVPROPKEY*, DEVPROPTYPE*, PBYTE, DWORD, PDWORD, DWORD);
typedef BOOL (__stdcall *CancelIoExFunction)(HANDLE, LPOVERLAPPED);
SetupDiGetDevicePropertyWFunction SetupDiGetDevicePropertyW = NULL;
CancelIoExFunction CancelIoEx = NULL;

// Global list of available serial ports
char portsEnumerated = 0;
char classInitialized = 0;
CRITICAL_SECTION criticalSection;
serialPortVector serialPorts = { NULL, 0, 0 };

// JNI exception handler
char jniErrorMessage[64] = { 0 };
int lastErrorLineNumber = 0, lastErrorNumber = 0;
static inline jboolean checkJniError(JNIEnv *env, int lineNumber)
{
	// Check if a JNI exception has been thrown
	if ((*env)->ExceptionCheck(env))
	{
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		snprintf(jniErrorMessage, sizeof(jniErrorMessage), "Native exception thrown at line %d", lineNumber);
		(*env)->ThrowNew(env, jniErrorClass, jniErrorMessage);
		return JNI_TRUE;
	}
	return JNI_FALSE;
}

// Generalized port enumeration function
static void enumeratePorts(void)
{
	// Reset the enumerated flag on all non-open serial ports
	for (int i = 0; i < serialPorts.length; ++i)
		serialPorts.ports[i]->enumerated = (serialPorts.ports[i]->handle != INVALID_HANDLE_VALUE);

	// Enumerate all serial ports present on the current system
	wchar_t *deviceID = NULL;
	DWORD deviceIdLength = 0;
	const struct { GUID guid; DWORD flags; } setupClasses[] = {
			{ .guid = GUID_DEVCLASS_PORTS, .flags = DIGCF_PRESENT },
			{ .guid = GUID_DEVCLASS_MODEM, .flags = DIGCF_PRESENT },
			{ .guid = GUID_DEVCLASS_MULTIPORTSERIAL, .flags = DIGCF_PRESENT },
			{ .guid = GUID_DEVINTERFACE_COMPORT, .flags = DIGCF_PRESENT | DIGCF_DEVICEINTERFACE },
			{ .guid = GUID_DEVINTERFACE_MODEM, .flags = DIGCF_PRESENT | DIGCF_DEVICEINTERFACE }
	};
	for (int i = 0; i < (sizeof(setupClasses) / sizeof(setupClasses[0])); ++i)
	{
		HDEVINFO devList = SetupDiGetClassDevsW(&setupClasses[i].guid, NULL, NULL, setupClasses[i].flags);
		if (devList != INVALID_HANDLE_VALUE)
		{
			// Iterate through all devices
			DWORD devInterfaceIndex = 0;
			DEVPROPTYPE devInfoPropType;
			SP_DEVINFO_DATA devInfoData;
			devInfoData.cbSize = sizeof(devInfoData);
			while (SetupDiEnumDeviceInfo(devList, devInterfaceIndex++, &devInfoData))
			{
				// Attempt to determine the device's Vendor ID and Product ID
				DWORD deviceIdRequiredLength;
				int vendorID = -1, productID = -1;
				if (!SetupDiGetDeviceInstanceIdW(devList, &devInfoData, NULL, 0, &deviceIdRequiredLength) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER) && (deviceIdRequiredLength > deviceIdLength))
				{
					wchar_t *newMemory = (wchar_t*)realloc(deviceID, deviceIdRequiredLength * sizeof(wchar_t));
					if (newMemory)
					{
						deviceID = newMemory;
						deviceIdLength = deviceIdRequiredLength;
					}
				}
				if (SetupDiGetDeviceInstanceIdW(devList, &devInfoData, deviceID, deviceIdLength, NULL))
				{
					wchar_t *vendorIdString = wcsstr(deviceID, L"VID_"), *productIdString = wcsstr(deviceID, L"PID_");
					if (vendorIdString && productIdString)
					{
						*wcschr(vendorIdString, L'&') = L'\0';
						vendorID = _wtoi(vendorIdString + 4);
						productID = _wtoi(productIdString + 4);
					}
				}

				// Fetch the corresponding COM port for this device
				DWORD comPortLength = 0;
				wchar_t *comPort = NULL, *comPortString = NULL;
				char friendlyNameMemory = 0, portDescriptionMemory = 0;
				HKEY key = SetupDiOpenDevRegKey(devList, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
				if (key != INVALID_HANDLE_VALUE)
				{
					if ((RegQueryValueExW(key, L"PortName", NULL, NULL, NULL, &comPortLength) == ERROR_SUCCESS) && (comPortLength < 32))
					{
						comPortLength += sizeof(wchar_t);
						comPort = (wchar_t*)malloc(comPortLength);
						if (comPort && (RegQueryValueExW(key, L"PortName", NULL, NULL, (LPBYTE)comPort, &comPortLength) == ERROR_SUCCESS))
							comPortString = (comPort[0] == L'\\') ? (wcsrchr(comPort, L'\\') + 1) : comPort;
					}
					RegCloseKey(key);
				}
				if (!comPortString || wcsstr(comPortString, L"LPT"))
				{
					if (comPort)
						free(comPort);
					continue;
				}

				// Fetch the friendly name for this device
				DWORD friendlyNameLength = 0;
				wchar_t *friendlyNameString = NULL;
				SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_FRIENDLYNAME, NULL, NULL, 0, &friendlyNameLength);
				if (friendlyNameLength && (friendlyNameLength < 256))
				{
					friendlyNameLength += sizeof(wchar_t);
					friendlyNameString = (wchar_t*)malloc(friendlyNameLength);
					if (!friendlyNameString || !SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_FRIENDLYNAME, NULL, (BYTE*)friendlyNameString, friendlyNameLength, NULL))
					{
						if (friendlyNameString)
							free(friendlyNameString);
						friendlyNameString = comPortString;
						friendlyNameLength = comPortLength;
					}
					else
					{
						friendlyNameMemory = 1;
						friendlyNameString[(friendlyNameLength / sizeof(wchar_t)) - 1] = 0;
					}
				}
				else
				{
					friendlyNameString = comPortString;
					friendlyNameLength = comPortLength;
				}

				// Fetch the bus-reported device description
				DWORD portDescriptionLength = 0;
				wchar_t *portDescriptionString = NULL;
				if (SetupDiGetDevicePropertyW && (SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &devInfoPropType, NULL, 0, &portDescriptionLength, 0) || (GetLastError() == ERROR_INSUFFICIENT_BUFFER)) && portDescriptionLength && (portDescriptionLength < 256))
				{
					portDescriptionLength += sizeof(wchar_t);
					portDescriptionString = (wchar_t*)malloc(portDescriptionLength);
					if (!portDescriptionString || !SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &devInfoPropType, (BYTE*)portDescriptionString, portDescriptionLength, NULL, 0))
					{
						if (portDescriptionString)
							free(portDescriptionString);
						portDescriptionString = friendlyNameString;
						portDescriptionLength = friendlyNameLength;
					}
					else
					{
						portDescriptionMemory = 1;
						portDescriptionString[(portDescriptionLength / sizeof(wchar_t)) - 1] = 0;
					}
				}
				else
				{
					portDescriptionString = friendlyNameString;
					portDescriptionLength = friendlyNameLength;
				}

				// Fetch the physical location for this device
				wchar_t *locationString = NULL;
				DWORD locationLength = 0, busNumber = -1, hubNumber = -1, portNumber = -1;
				if (!SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_BUSNUMBER, NULL, (BYTE*)&busNumber, sizeof(busNumber), NULL))
					busNumber = -1;
				if (!SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_ADDRESS, NULL, (BYTE*)&portNumber, sizeof(portNumber), NULL))
					portNumber = -1;
				SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_LOCATION_INFORMATION, NULL, NULL, 0, &locationLength);
				if (locationLength && (locationLength < 256))
				{
					locationLength += sizeof(wchar_t);
					locationString = (wchar_t*)malloc(locationLength);
					if (locationString && SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_LOCATION_INFORMATION, NULL, (BYTE*)locationString, locationLength, NULL))
					{
						locationString[(locationLength / sizeof(wchar_t)) - 1] = 0;
						if (wcsstr(locationString, L"Hub"))
							hubNumber = _wtoi(wcschr(wcsstr(locationString, L"Hub"), L'#') + 1);
						if ((portNumber == -1) && wcsstr(locationString, L"Port"))
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
					if (locationString)
						free(locationString);
				}
				if (busNumber == -1)
					busNumber = 0;
				if (hubNumber == -1)
					hubNumber = 0;
				if (portNumber == -1)
					portNumber = 0;
				locationString = (wchar_t*)malloc(16*sizeof(wchar_t));
				if (locationString)
					_snwprintf_s(locationString, 16, 16, L"%d-%d.%d", busNumber, hubNumber, portNumber);
				else
				{
					free(comPort);
					if (friendlyNameMemory)
						free(friendlyNameString);
					if (portDescriptionMemory)
						free(portDescriptionString);
					continue;
				}

				// Check if port is already enumerated
				serialPort *port = fetchPort(&serialPorts, comPortString);
				if (port)
				{
					// See if device has changed locations
					port->enumerated = 1;
					int oldLength = 1 + wcslen(port->portLocation);
					int newLength = 1 + wcslen(locationString);
					if (oldLength != newLength)
					{
						wchar_t *newMemory = (wchar_t*)realloc(port->portLocation, newLength * sizeof(wchar_t));
						if (newMemory)
						{
							port->portLocation = newMemory;
							wcscpy_s(port->portLocation, newLength, locationString);
						}
						else
							wcscpy_s(port->portLocation, oldLength, locationString);
					}
					else if (wcscmp(port->portLocation, locationString))
						wcscpy_s(port->portLocation, newLength, locationString);
				}
				else
					pushBack(&serialPorts, comPortString, friendlyNameString, portDescriptionString, locationString, vendorID, productID);

				// Clean up memory and reset device info structure
				free(comPort);
				free(locationString);
				if (friendlyNameMemory)
					free(friendlyNameString);
				if (portDescriptionMemory)
					free(portDescriptionString);
				devInfoData.cbSize = sizeof(devInfoData);
			}
			SetupDiDestroyDeviceInfoList(devList);
		}
	}

	// Attempt to locate any FTDI-specified port descriptions
	HINSTANCE ftdiLibInstance = LoadLibrary(TEXT("ftd2xx.dll"));
	if (ftdiLibInstance != NULL)
	{
		FT_CreateDeviceInfoListFunction FT_CreateDeviceInfoList = (FT_CreateDeviceInfoListFunction)GetProcAddress(ftdiLibInstance, "FT_CreateDeviceInfoList");
		FT_GetDeviceInfoListFunction FT_GetDeviceInfoList = (FT_GetDeviceInfoListFunction)GetProcAddress(ftdiLibInstance, "FT_GetDeviceInfoList");
		if (FT_CreateDeviceInfoList && FT_GetDeviceInfoList)
		{
			DWORD numDevs;
			if ((FT_CreateDeviceInfoList(&numDevs) == FT_OK) && (numDevs > 0))
			{
				FT_DEVICE_LIST_INFO_NODE *devInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*numDevs);
				if (devInfo && (FT_GetDeviceInfoList(devInfo, &numDevs) == FT_OK))
				{
					for (int i = 0; i < numDevs; ++i)
					{
						// Determine if the port is currently enumerated and already open
						char isOpen = ((devInfo[i].Flags & FT_FLAGS_OPENED) || (devInfo[i].SerialNumber[0] == 0)) ? 1 : 0;
						if (!isOpen)
							for (int j = 0; j < serialPorts.length; ++j)
								if ((memcmp(serialPorts.ports[j]->serialNumber, devInfo[i].SerialNumber, sizeof(serialPorts.ports[j]->serialNumber)) == 0) && (serialPorts.ports[j]->handle != INVALID_HANDLE_VALUE))
								{
									serialPorts.ports[j]->enumerated = 1;
									isOpen = 1;
									break;
								}

						// Update the port description if not already open
						const int comPortLength = 16;
						wchar_t *comPort = (wchar_t*)malloc(comPortLength);
						devInfo[i].Description[sizeof(devInfo[i].Description)-1] = 0;
						devInfo[i].SerialNumber[sizeof(devInfo[i].SerialNumber)-1] = 0;
						if (!isOpen && comPort && getPortPathFromSerial(comPort, comPortLength, devInfo[i].SerialNumber))
						{
							// Check if actually connected and present in the port list
							for (int j = 0; j < serialPorts.length; ++j)
								if ((wcscmp(serialPorts.ports[j]->portPath + 4, comPort) == 0) && strlen(devInfo[i].Description))
								{
									// Update the port description
									serialPorts.ports[j]->enumerated = 1;
									size_t descLength = 8 + strlen(devInfo[i].Description);
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
						if (comPort)
							free(comPort);
					}
				}
				if (devInfo)
					free(devInfo);
			}
		}
		FreeLibrary(ftdiLibInstance);
	}

	// Attempt to locate any non-registered virtual serial ports (e.g., from VSPE)
	HKEY key, paramKey;
	DWORD keyType, numValues, maxValueLength, maxComPortLength;
	if ((RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) &&
			(RegQueryInfoKeyW(key, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, &maxValueLength, &maxComPortLength, NULL, NULL) == ERROR_SUCCESS))
	{
		// Allocate memory
		++maxValueLength;
		++maxComPortLength;
		WCHAR *valueName = (WCHAR*)malloc(maxValueLength*sizeof(WCHAR));
		WCHAR *comPort = (WCHAR*)malloc(maxComPortLength*sizeof(WCHAR));

		// Iterate through all COM ports
		for (DWORD i = 0; i < numValues; ++i)
		{
			// Get serial port name and COM value
			DWORD valueLength = maxValueLength;
			DWORD comPortLength = maxComPortLength;
			memset(valueName, 0, valueLength*sizeof(WCHAR));
			memset(comPort, 0, comPortLength*sizeof(WCHAR));
			if ((RegEnumValueW(key, i, valueName, &valueLength, NULL, &keyType, (BYTE*)comPort, &comPortLength) == ERROR_SUCCESS) && (keyType == REG_SZ))
			{
				// Set port name and description
				wchar_t* comPortString = (comPort[0] == L'\\') ? (wcsrchr(comPort, L'\\') + 1) : comPort;
				wchar_t* friendlyNameString = wcsrchr(valueName, L'\\') ? (wcsrchr(valueName, L'\\') + 1) : valueName;

				// Add new SerialComm object to vector if it does not already exist
				serialPort *port = fetchPort(&serialPorts, comPortString);
				if (port)
					port->enumerated = 1;
				else
					pushBack(&serialPorts, comPortString, friendlyNameString, L"Virtual Serial Port", L"X-X.X", -1, -1);
			}
		}

		// Clean up memory
		free(valueName);
		free(comPort);
		RegCloseKey(key);
	}

	// Clean up memory
	if (deviceID)
		free(deviceID);

	// Remove all non-enumerated ports from the serial port listing
	for (int i = 0; i < serialPorts.length; ++i)
		if (!serialPorts.ports[i]->enumerated)
		{
			removePort(&serialPorts, serialPorts.ports[i]);
			i--;
		}
	portsEnumerated = 1;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved)
{
	// Retrieve the JNI environment and class
	JNIEnv *env;
	jint jniVersion = JNI_VERSION_1_2;
	if ((*jvm)->GetEnv(jvm, (void**)&env, jniVersion))
		return JNI_ERR;
	jclass serialCommClass = (*env)->FindClass(env, "com/fazecast/jSerialComm/SerialPort");
	if (!serialCommClass) return JNI_ERR;
	jniErrorClass = (*env)->FindClass(env, "java/lang/Exception");
	if (!jniErrorClass) return JNI_ERR;

	// Cache Java fields as global references
	serialCommConstructor = (*env)->GetMethodID(env, serialCommClass, "<init>", "()V");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	serialPortHandleField = (*env)->GetFieldID(env, serialCommClass, "portHandle", "J");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	comPortField = (*env)->GetFieldID(env, serialCommClass, "comPort", "Ljava/lang/String;");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	friendlyNameField = (*env)->GetFieldID(env, serialCommClass, "friendlyName", "Ljava/lang/String;");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	portDescriptionField = (*env)->GetFieldID(env, serialCommClass, "portDescription", "Ljava/lang/String;");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	vendorIdField = (*env)->GetFieldID(env, serialCommClass, "vendorID", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	productIdField = (*env)->GetFieldID(env, serialCommClass, "productID", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	portLocationField = (*env)->GetFieldID(env, serialCommClass, "portLocation", "Ljava/lang/String;");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	eventListenerRunningField = (*env)->GetFieldID(env, serialCommClass, "eventListenerRunning", "Z");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	disableConfigField = (*env)->GetFieldID(env, serialCommClass, "disableConfig", "Z");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	isDtrEnabledField = (*env)->GetFieldID(env, serialCommClass, "isDtrEnabled", "Z");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	isRtsEnabledField = (*env)->GetFieldID(env, serialCommClass, "isRtsEnabled", "Z");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	autoFlushIOBuffersField = (*env)->GetFieldID(env, serialCommClass, "autoFlushIOBuffers", "Z");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	baudRateField = (*env)->GetFieldID(env, serialCommClass, "baudRate", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	dataBitsField = (*env)->GetFieldID(env, serialCommClass, "dataBits", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	stopBitsField = (*env)->GetFieldID(env, serialCommClass, "stopBits", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	parityField = (*env)->GetFieldID(env, serialCommClass, "parity", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	flowControlField = (*env)->GetFieldID(env, serialCommClass, "flowControl", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	sendDeviceQueueSizeField = (*env)->GetFieldID(env, serialCommClass, "sendDeviceQueueSize", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	receiveDeviceQueueSizeField = (*env)->GetFieldID(env, serialCommClass, "receiveDeviceQueueSize", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	requestElevatedPermissionsField = (*env)->GetFieldID(env, serialCommClass, "requestElevatedPermissions", "Z");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	rs485ModeField = (*env)->GetFieldID(env, serialCommClass, "rs485Mode", "Z");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	rs485DelayBeforeField = (*env)->GetFieldID(env, serialCommClass, "rs485DelayBefore", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	rs485DelayAfterField = (*env)->GetFieldID(env, serialCommClass, "rs485DelayAfter", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	xonStartCharField = (*env)->GetFieldID(env, serialCommClass, "xonStartChar", "B");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	xoffStopCharField = (*env)->GetFieldID(env, serialCommClass, "xoffStopChar", "B");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	timeoutModeField = (*env)->GetFieldID(env, serialCommClass, "timeoutMode", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	readTimeoutField = (*env)->GetFieldID(env, serialCommClass, "readTimeout", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	writeTimeoutField = (*env)->GetFieldID(env, serialCommClass, "writeTimeout", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	eventFlagsField = (*env)->GetFieldID(env, serialCommClass, "eventFlags", "I");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;

	// Attempt to load Windows functions that may or may not be present on every PC
	HINSTANCE setupApiInstance = LoadLibrary(TEXT("SetupAPI.dll"));
	HINSTANCE kernelInstance = LoadLibrary(TEXT("Kernel32.dll"));
	if (setupApiInstance != NULL)
		SetupDiGetDevicePropertyW = (SetupDiGetDevicePropertyWFunction)GetProcAddress(setupApiInstance, "SetupDiGetDevicePropertyW");
	if (kernelInstance != NULL)
		CancelIoEx = (CancelIoExFunction)GetProcAddress(kernelInstance, "CancelIoEx");

	// Initialize the critical section lock
	InitializeCriticalSection(&criticalSection);
	classInitialized = 1;
	return jniVersion;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *jvm, void *reserved)
{
	// Ensure that the class has not already been uninitialized
	if (!classInitialized)
		return;
	classInitialized = 0;

	// Retrieve the JNI environment
	JNIEnv *env;
	jint jniVersion = JNI_VERSION_1_2;
	(*jvm)->GetEnv(jvm, (void**)&env, jniVersion);

	// Close all open ports
	for (int i = 0; i < serialPorts.length; ++i)
		if (serialPorts.ports[i]->handle != INVALID_HANDLE_VALUE)
			Java_com_fazecast_jSerialComm_SerialPort_closePortNative(env, jniErrorClass, (jlong)(intptr_t)serialPorts.ports[i]);
}

JNIEXPORT void JNICALL Java_com_fazecast_jSerialComm_SerialPort_uninitializeLibrary(JNIEnv *env, jclass serialComm)
{
	// Call the JNI Unload function
	JavaVM *jvm;
	(*env)->GetJavaVM(env, &jvm);
	JNI_OnUnload(jvm, NULL);
}

JNIEXPORT jobjectArray JNICALL Java_com_fazecast_jSerialComm_SerialPort_getCommPortsNative(JNIEnv *env, jclass serialComm)
{
	// Mark this entire function as a critical section
	EnterCriticalSection(&criticalSection);

	// Enumerate all ports on the current system
	enumeratePorts();

	// Get relevant SerialComm methods and fill in com port array
	jobjectArray arrayObject = (*env)->NewObjectArray(env, serialPorts.length, serialComm, 0);
	for (int i = 0; !checkJniError(env, __LINE__ - 1) && (i < serialPorts.length); ++i)
	{
		// Create new SerialComm object containing the enumerated values
		jobject serialCommObject = (*env)->NewObject(env, serialComm, serialCommConstructor);
		if (checkJniError(env, __LINE__ - 1)) break;
		(*env)->SetObjectField(env, serialCommObject, comPortField, (*env)->NewString(env, (jchar*)serialPorts.ports[i]->portPath, wcslen(serialPorts.ports[i]->portPath)));
		if (checkJniError(env, __LINE__ - 1)) break;
		(*env)->SetObjectField(env, serialCommObject, friendlyNameField, (*env)->NewString(env, (jchar*)serialPorts.ports[i]->friendlyName, wcslen(serialPorts.ports[i]->friendlyName)));
		if (checkJniError(env, __LINE__ - 1)) break;
		(*env)->SetObjectField(env, serialCommObject, portDescriptionField, (*env)->NewString(env, (jchar*)serialPorts.ports[i]->portDescription, wcslen(serialPorts.ports[i]->portDescription)));
		if (checkJniError(env, __LINE__ - 1)) break;
		(*env)->SetObjectField(env, serialCommObject, portLocationField, (*env)->NewString(env, (jchar*)serialPorts.ports[i]->portLocation, wcslen(serialPorts.ports[i]->portLocation)));
		if (checkJniError(env, __LINE__ - 1)) break;
		(*env)->SetIntField(env, serialCommObject, vendorIdField, serialPorts.ports[i]->vendorID);
		if (checkJniError(env, __LINE__ - 1)) break;
		(*env)->SetIntField(env, serialCommObject, productIdField, serialPorts.ports[i]->productID);
		if (checkJniError(env, __LINE__ - 1)) break;

		// Add new SerialComm object to array
		(*env)->SetObjectArrayElement(env, arrayObject, i, serialCommObject);
	}

	// Exit critical section and return the com port array
	LeaveCriticalSection(&criticalSection);
	return arrayObject;
}

JNIEXPORT void JNICALL Java_com_fazecast_jSerialComm_SerialPort_retrievePortDetails(JNIEnv *env, jobject obj)
{
	// Retrieve the serial port parameter fields
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	if (checkJniError(env, __LINE__ - 1)) return;
	const wchar_t *portName = (wchar_t*)(*env)->GetStringChars(env, portNameJString, NULL);
	if (checkJniError(env, __LINE__ - 1)) return;

	// Ensure that the serial port exists
	char continueRetrieval = 1;
	EnterCriticalSection(&criticalSection);
	if (!portsEnumerated)
		enumeratePorts();
	serialPort *port = fetchPort(&serialPorts, portName);
	if (!port)
		continueRetrieval = 0;

	// Fill in the Java-side port details
	if (continueRetrieval)
	{
		(*env)->SetObjectField(env, obj, friendlyNameField, (*env)->NewString(env, (jchar*)port->friendlyName, wcslen(port->friendlyName)));
		if (checkJniError(env, __LINE__ - 1)) continueRetrieval = 0;
	}
	if (continueRetrieval)
	{
		(*env)->SetObjectField(env, obj, portDescriptionField, (*env)->NewString(env, (jchar*)port->portDescription, wcslen(port->portDescription)));
		if (checkJniError(env, __LINE__ - 1)) continueRetrieval = 0;
	}
	if (continueRetrieval)
	{
		(*env)->SetObjectField(env, obj, portLocationField, (*env)->NewString(env, (jchar*)port->portLocation, wcslen(port->portLocation)));
		if (checkJniError(env, __LINE__ - 1)) continueRetrieval = 0;
	}
	if (continueRetrieval)
	{
		(*env)->SetIntField(env, obj, vendorIdField, port->vendorID);
		if (checkJniError(env, __LINE__ - 1)) continueRetrieval = 0;
	}
	if (continueRetrieval)
	{
		(*env)->SetIntField(env, obj, productIdField, port->productID);
		if (checkJniError(env, __LINE__ - 1)) continueRetrieval = 0;
	}

	// Release all JNI structures
	LeaveCriticalSection(&criticalSection);
	(*env)->ReleaseStringChars(env, portNameJString, (const jchar*)portName);
	checkJniError(env, __LINE__ - 1);
}

JNIEXPORT jlong JNICALL Java_com_fazecast_jSerialComm_SerialPort_openPortNative(JNIEnv *env, jobject obj)
{
	// Retrieve the serial port parameter fields
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	if (checkJniError(env, __LINE__ - 1)) return 0;
	unsigned char requestElevatedPermissions = (*env)->GetBooleanField(env, obj, requestElevatedPermissionsField);
	if (checkJniError(env, __LINE__ - 1)) return 0;
	unsigned char disableAutoConfig = (*env)->GetBooleanField(env, obj, disableConfigField);
	if (checkJniError(env, __LINE__ - 1)) return 0;
	unsigned char autoFlushIOBuffers = (*env)->GetBooleanField(env, obj, autoFlushIOBuffersField);
	if (checkJniError(env, __LINE__ - 1)) return 0;
	unsigned char isDtrEnabled = (*env)->GetBooleanField(env, obj, isDtrEnabledField);
	if (checkJniError(env, __LINE__ - 1)) return 0;
	unsigned char isRtsEnabled = (*env)->GetBooleanField(env, obj, isRtsEnabledField);
	if (checkJniError(env, __LINE__ - 1)) return 0;
	const wchar_t *portName = (wchar_t*)(*env)->GetStringChars(env, portNameJString, NULL);
	if (checkJniError(env, __LINE__ - 1)) return 0;

	// Ensure that the serial port still exists and is not already open
	EnterCriticalSection(&criticalSection);
	serialPort *port = fetchPort(&serialPorts, portName);
	if (!port)
	{
		// Create port representation and add to serial port listing
		port = pushBack(&serialPorts, portName, L"User-Specified Port", L"User-Specified Port", L"0-0", -1, -1);
	}
	LeaveCriticalSection(&criticalSection);
	if (!port || (port->handle != INVALID_HANDLE_VALUE))
	{
		(*env)->ReleaseStringChars(env, portNameJString, (const jchar*)portName);
		checkJniError(env, __LINE__ - 1);
		lastErrorLineNumber = __LINE__ - 3;
		lastErrorNumber = (!port ? 1 : 2);
		return 0;
	}

	// Reduce the port's latency to its minimum value
	reduceLatencyToMinimum(portName + 4, requestElevatedPermissions);

	// Try to open the serial port with read/write access
	port->errorLineNumber = lastErrorLineNumber = __LINE__ + 1;
	void *portHandle = CreateFileW(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED, NULL);
	if (portHandle != INVALID_HANDLE_VALUE)
	{
		// Set the newly opened port handle in the serial port structure
		EnterCriticalSection(&criticalSection);
		port->handle = portHandle;
		LeaveCriticalSection(&criticalSection);

		// Quickly set the desired RTS/DTR line status immediately upon opening
		if (isDtrEnabled)
			Java_com_fazecast_jSerialComm_SerialPort_setDTR(env, obj, (jlong)(intptr_t)port);
		else
			Java_com_fazecast_jSerialComm_SerialPort_clearDTR(env, obj, (jlong)(intptr_t)port);
		if (isRtsEnabled)
			Java_com_fazecast_jSerialComm_SerialPort_setRTS(env, obj, (jlong)(intptr_t)port);
		else
			Java_com_fazecast_jSerialComm_SerialPort_clearRTS(env, obj, (jlong)(intptr_t)port);

		// Configure the port parameters and timeouts
		if (!disableAutoConfig && !Java_com_fazecast_jSerialComm_SerialPort_configPort(env, obj, (jlong)(intptr_t)port))
		{
			// Close the port if there was a problem setting the parameters
			PurgeComm(port->handle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
			if (CancelIoEx)
				CancelIoEx(port->handle, NULL);
			SetCommMask(port->handle, 0);
			CloseHandle(port->handle);
			EnterCriticalSection(&criticalSection);
			port->handle = INVALID_HANDLE_VALUE;
			LeaveCriticalSection(&criticalSection);
		}
		else if (autoFlushIOBuffers)
			Java_com_fazecast_jSerialComm_SerialPort_flushRxTxBuffers(env, obj, (jlong)(intptr_t)port);
	}
	else
		port->errorNumber = lastErrorNumber = GetLastError();

	// Return a pointer to the serial port data structure
	(*env)->ReleaseStringChars(env, portNameJString, (const jchar*)portName);
	checkJniError(env, __LINE__ - 1);
	return (port->handle != INVALID_HANDLE_VALUE) ? (jlong)(intptr_t)port : 0;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configPort(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Retrieve port parameters from the Java class
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	DWORD baudRate = (DWORD)(*env)->GetIntField(env, obj, baudRateField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	BYTE byteSize = (BYTE)(*env)->GetIntField(env, obj, dataBitsField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	int stopBitsInt = (*env)->GetIntField(env, obj, stopBitsField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	int parityInt = (*env)->GetIntField(env, obj, parityField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	int flowControl = (*env)->GetIntField(env, obj, flowControlField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	int timeoutMode = (*env)->GetIntField(env, obj, timeoutModeField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	int readTimeout = (*env)->GetIntField(env, obj, readTimeoutField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	int writeTimeout = (*env)->GetIntField(env, obj, writeTimeoutField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	int eventsToMonitor = (*env)->GetIntField(env, obj, eventFlagsField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	char xonStartChar = (*env)->GetByteField(env, obj, xonStartCharField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	char xoffStopChar = (*env)->GetByteField(env, obj, xoffStopCharField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	DWORD sendDeviceQueueSize = (DWORD)(*env)->GetIntField(env, obj, sendDeviceQueueSizeField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	DWORD receiveDeviceQueueSize = (DWORD)(*env)->GetIntField(env, obj, receiveDeviceQueueSizeField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	BYTE rs485ModeEnabled = (BYTE)(*env)->GetBooleanField(env, obj, rs485ModeField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	BYTE isDtrEnabled = (*env)->GetBooleanField(env, obj, isDtrEnabledField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	BYTE isRtsEnabled = (*env)->GetBooleanField(env, obj, isRtsEnabledField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
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
		port->errorLineNumber = lastErrorLineNumber = __LINE__ - 2;
		port->errorNumber = lastErrorNumber = GetLastError();
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
		port->errorLineNumber = lastErrorLineNumber = __LINE__ - 2;
		port->errorNumber = lastErrorNumber = GetLastError();
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
	if (eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_BREAK_INTERRUPT)
		eventFlags |= EV_BREAK;
	if (eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_CTS)
		eventFlags |= EV_CTS;
	if (eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DSR)
		eventFlags |= EV_DSR;
	if (eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_RING_INDICATOR)
		eventFlags |= EV_RING;
	if (eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_CARRIER_DETECT)
		eventFlags |= EV_RLSD;

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
		port->errorLineNumber = lastErrorLineNumber = __LINE__ - 2;
		port->errorNumber = lastErrorNumber = GetLastError();
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
			event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PORT_DISCONNECTED;
			port->errorNumber = GetLastError();
			port->errorLineNumber = __LINE__ - 18;
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
	if (eventMask & EV_BREAK)
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_BREAK_INTERRUPT;
	if (eventMask & EV_TXEMPTY)
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_WRITTEN;
	if ((eventMask & EV_RXCHAR) && (commInfo.cbInQue > 0))
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE;
	if (eventMask & EV_CTS)
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_CTS;
	if (eventMask & EV_DSR)
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DSR;
	if (eventMask & EV_RING)
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_RING_INDICATOR;
	if (eventMask & EV_RLSD)
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
	if (CancelIoEx)
		CancelIoEx(port->handle, NULL);
	SetCommMask(port->handle, 0);

	// Close the port
	port->eventListenerRunning = 0;
	port->errorLineNumber = lastErrorLineNumber = __LINE__ + 1;
	port->errorNumber = lastErrorNumber = (!CloseHandle(port->handle) ? GetLastError() : 0);
	EnterCriticalSection(&criticalSection);
	port->handle = INVALID_HANDLE_VALUE;
	LeaveCriticalSection(&criticalSection);
	return 0;
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
	checkJniError(env, __LINE__ - 1);
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
	if (checkJniError(env, __LINE__ - 1))
		return -1;
	else if (((result = WriteFile(port->handle, writeBuffer+offset, bytesToWrite, NULL, &overlappedStruct)) == FALSE) && (GetLastError() != ERROR_IO_PENDING))
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
	checkJniError(env, __LINE__ - 1);
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
	return serialPortPointer ? ((serialPort*)(intptr_t)serialPortPointer)->errorLineNumber : lastErrorLineNumber;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_getLastErrorCode(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	return serialPortPointer ? ((serialPort*)(intptr_t)serialPortPointer)->errorNumber : lastErrorNumber;
}

#endif
