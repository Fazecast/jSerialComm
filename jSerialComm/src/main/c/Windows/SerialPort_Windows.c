/*
 * SerialPort_Windows.c
 *
 *       Created on:  Feb 25, 2012
 *  Last Updated on:  Apr 01, 2020
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2020 Fazecast, Inc.
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
#include "ftdi/ftd2xx.h"
#include "../com_fazecast_jSerialComm_SerialPort.h"
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
jfieldID timeoutModeField;
jfieldID readTimeoutField;
jfieldID writeTimeoutField;
jfieldID eventFlagsField;

// Run-time loadable DLL function
typedef int (__stdcall *FT_CreateDeviceInfoListFunction)(LPDWORD);
typedef int (__stdcall *FT_GetDeviceInfoListFunction)(FT_DEVICE_LIST_INFO_NODE*, LPDWORD);
typedef int (__stdcall *FT_GetComPortNumberFunction)(FT_HANDLE, LPLONG);
typedef int (__stdcall *FT_OpenFunction)(int, FT_HANDLE*);
typedef int (__stdcall *FT_CloseFunction)(FT_HANDLE);

JNIEXPORT jobjectArray JNICALL Java_com_fazecast_jSerialComm_SerialPort_getCommPorts(JNIEnv *env, jclass serialComm)
{
	HKEY keyHandle1, keyHandle2, keyHandle3, keyHandle4, keyHandle5;
	DWORD numSubKeys1, numSubKeys2, numSubKeys3, numValues;
	DWORD maxSubKeyLength1, maxSubKeyLength2, maxSubKeyLength3;
	DWORD maxValueLength, maxComPortLength, valueLength, comPortLength, keyType;
	DWORD subKeyLength1, subKeyLength2, subKeyLength3, friendlyNameLength;

	// Enumerate serial ports on machine
	charTupleVector serialCommPorts = { (wchar_t**)malloc(sizeof(wchar_t*)), (wchar_t**)malloc(sizeof(wchar_t*)), (wchar_t**)malloc(sizeof(wchar_t*)), 0 };
	if ((RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_QUERY_VALUE, &keyHandle1) == ERROR_SUCCESS) &&
			(RegQueryInfoKeyW(keyHandle1, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, &maxValueLength, &maxComPortLength, NULL, NULL) == ERROR_SUCCESS))
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
			valueLength = maxValueLength;
			comPortLength = maxComPortLength;
			memset(valueName, 0, valueLength*sizeof(WCHAR));
			memset(comPort, 0, comPortLength*sizeof(WCHAR));
			if ((RegEnumValueW(keyHandle1, i, valueName, &valueLength, NULL, &keyType, (BYTE*)comPort, &comPortLength) == ERROR_SUCCESS) && (keyType == REG_SZ))
			{
				// Set port name and description
				wchar_t* comPortString = (comPort[0] == L'\\') ? (wcsrchr(comPort, L'\\') + 1) : comPort;
				wchar_t* descriptionString = wcsrchr(valueName, L'\\') ? (wcsrchr(valueName, L'\\') + 1) : valueName;

				// Add new SerialComm object to vector
				push_back(&serialCommPorts, comPortString, descriptionString, descriptionString);
			}
		}

		// Clean up memory
		free(valueName);
		free(comPort);
	}
	RegCloseKey(keyHandle1);

	// Enumerate all devices on machine
	if ((RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Enum", 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &keyHandle1) == ERROR_SUCCESS) &&
			(RegQueryInfoKeyW(keyHandle1, NULL, NULL, NULL, &numSubKeys1, &maxSubKeyLength1, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS))
	{
		// Allocate memory
		++maxSubKeyLength1;
		WCHAR *subKeyName1 = (WCHAR*)malloc(maxSubKeyLength1*sizeof(WCHAR));

		// Enumerate sub-keys
		for (DWORD i1 = 0; i1 < numSubKeys1; ++i1)
		{
			subKeyLength1 = maxSubKeyLength1;
			if ((RegEnumKeyExW(keyHandle1, i1, subKeyName1, &subKeyLength1, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) &&
					(RegOpenKeyExW(keyHandle1, subKeyName1, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &keyHandle2) == ERROR_SUCCESS) &&
					(RegQueryInfoKeyW(keyHandle2, NULL, NULL, NULL, &numSubKeys2, &maxSubKeyLength2, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS))
			{
				// Allocate memory
				++maxSubKeyLength2;
				WCHAR *subKeyName2 = (WCHAR*)malloc(maxSubKeyLength2*sizeof(WCHAR));

				// Enumerate sub-keys
				for (DWORD i2 = 0; i2 < numSubKeys2; ++i2)
				{
					subKeyLength2 = maxSubKeyLength2;
					if ((RegEnumKeyExW(keyHandle2, i2, subKeyName2, &subKeyLength2, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) &&
							(RegOpenKeyExW(keyHandle2, subKeyName2, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &keyHandle3) == ERROR_SUCCESS) &&
							(RegQueryInfoKeyW(keyHandle3, NULL, NULL, NULL, &numSubKeys3, &maxSubKeyLength3, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS))
					{
						// Allocate memory
						++maxSubKeyLength3;
						WCHAR *subKeyName3 = (WCHAR*)malloc(maxSubKeyLength3*sizeof(WCHAR));

						// Enumerate sub-keys
						for (DWORD i3 = 0; i3 < numSubKeys3; ++i3)
						{
							subKeyLength3 = maxSubKeyLength3;
							if ((RegEnumKeyExW(keyHandle3, i3, subKeyName3, &subKeyLength3, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) &&
									(RegOpenKeyExW(keyHandle3, subKeyName3, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &keyHandle4) == ERROR_SUCCESS) &&
									(RegQueryInfoKeyW(keyHandle4, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, NULL, &valueLength, NULL, NULL) == ERROR_SUCCESS))
							{
								// Allocate memory
								friendlyNameLength = valueLength + 1;
								WCHAR *friendlyName = (WCHAR*)malloc(friendlyNameLength*sizeof(WCHAR));

								if ((RegOpenKeyExW(keyHandle4, L"Device Parameters", 0, KEY_QUERY_VALUE, &keyHandle5) == ERROR_SUCCESS) &&
									(RegQueryInfoKeyW(keyHandle5, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, NULL, &valueLength, NULL, NULL) == ERROR_SUCCESS))
								{
									// Allocate memory
									comPortLength = valueLength + 1;
									WCHAR *comPort = (WCHAR*)malloc(comPortLength*sizeof(WCHAR));

									// Attempt to get COM value and friendly port name
									if ((RegQueryValueExW(keyHandle5, L"PortName", NULL, &keyType, (BYTE*)comPort, &comPortLength) == ERROR_SUCCESS) && (keyType == REG_SZ) &&
											(RegQueryValueExW(keyHandle4, L"FriendlyName", NULL, &keyType, (BYTE*)friendlyName, &friendlyNameLength) == ERROR_SUCCESS) && (keyType == REG_SZ))
									{
										// Set port name and description
										wchar_t* comPortString = (comPort[0] == L'\\') ? (wcsrchr(comPort, L'\\') + 1) : comPort;
										wchar_t* descriptionString = friendlyName;

										// Update friendly name if COM port is actually connected and present in the port list
										int i;
										for (i = 0; i < serialCommPorts.length; ++i)
											if (wcscmp(serialCommPorts.first[i], comPortString) == 0)
											{
												free(serialCommPorts.second[i]);
												serialCommPorts.second[i] = (wchar_t*)malloc((wcslen(descriptionString)+1)*sizeof(wchar_t));
												wcscpy(serialCommPorts.second[i], descriptionString);
												break;
											}
									}

									// Clean up memory
									free(comPort);
								}

								// Clean up memory and close registry key
								RegCloseKey(keyHandle5);
								free(friendlyName);
							}

							// Close registry key
							RegCloseKey(keyHandle4);
						}

						// Clean up memory and close registry key
						RegCloseKey(keyHandle3);
						free(subKeyName3);
					}
				}

				// Clean up memory and close registry key
				RegCloseKey(keyHandle2);
				free(subKeyName2);
			}
		}

		// Clean up memory and close registry key
		RegCloseKey(keyHandle1);
		free(subKeyName1);
	}

	// Attempt to locate any device-specified port descriptions
	HDEVINFO devList = SetupDiGetClassDevsW(NULL, L"USB", NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (devList != INVALID_HANDLE_VALUE)
	{
		// Iterate through all USB-connected devices
		DWORD devInterfaceIndex = 0;
		DEVPROPTYPE devInfoPropType;
		SP_DEVINFO_DATA devInfoData;
		devInfoData.cbSize = sizeof(devInfoData);
		WCHAR comPort[128];
		while (SetupDiEnumDeviceInfo(devList, devInterfaceIndex++, &devInfoData))
		{
			// Fetch the corresponding COM port for this device
			wchar_t* comPortString = NULL;
			comPortLength = sizeof(comPort) / sizeof(WCHAR);
			keyHandle5 = SetupDiOpenDevRegKey(devList, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
			if ((keyHandle5 != INVALID_HANDLE_VALUE) && (RegQueryValueExW(keyHandle5, L"PortName", NULL, &keyType, (BYTE*)comPort, &comPortLength) == ERROR_SUCCESS) && (keyType == REG_SZ))
				comPortString = (comPort[0] == L'\\') ? (wcsrchr(comPort, L'\\') + 1) : comPort;
			if (keyHandle5 != INVALID_HANDLE_VALUE)
				RegCloseKey(keyHandle5);

			// Fetch the length of the "Bus-Reported Device Description"
			if (comPortString && !SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &devInfoPropType, NULL, 0, &valueLength, 0)  && (GetLastError() == ERROR_INSUFFICIENT_BUFFER))
			{
				// Allocate memory
				++valueLength;
				WCHAR *portDescription = (WCHAR*)malloc(valueLength);

				// Retrieve the "Bus-Reported Device Description"
				if (SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &devInfoPropType, (BYTE*)portDescription, valueLength, NULL, 0))
				{
					// Update port description if COM port is actually connected and present in the port list
					int i;
					for (i = 0; i < serialCommPorts.length; ++i)
						if (wcscmp(serialCommPorts.first[i], comPortString) == 0)
						{
							free(serialCommPorts.third[i]);
							serialCommPorts.third[i] = (wchar_t*)malloc((wcslen(portDescription)+1)*sizeof(wchar_t));
							wcscpy(serialCommPorts.third[i], portDescription);
							break;
						}
				}

				// Clean up memory
				free(portDescription);
			}
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
		if ((FT_CreateDeviceInfoList != NULL) && (FT_GetDeviceInfoList != NULL) && (FT_GetComPortNumber != NULL) && (FT_Open != NULL) && (FT_Close != NULL))
		{
			DWORD numDevs;
			if ((FT_CreateDeviceInfoList(&numDevs) == FT_OK) && (numDevs > 0))
			{
				FT_DEVICE_LIST_INFO_NODE *devInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*numDevs);
				if (FT_GetDeviceInfoList(devInfo, &numDevs) == FT_OK)
				{
					int i, j;
					wchar_t comPortString[128];
					for (i = 0; i < numDevs; ++i)
					{
						LONG comPortNumber = 0;
						if ((FT_Open(i, &devInfo[i].ftHandle) == FT_OK) && (FT_GetComPortNumber(devInfo[i].ftHandle, &comPortNumber) == FT_OK))
						{
							// Update port description if COM port is actually connected and present in the port list
							FT_Close(devInfo[i].ftHandle);
							swprintf(comPortString, sizeof(comPortString) / sizeof(wchar_t), L"COM%ld", comPortNumber);
							for (j = 0; j < serialCommPorts.length; ++j)
								if (wcscmp(serialCommPorts.first[j], comPortString) == 0)
								{
									size_t descLength = 8+strlen(devInfo[i].Description);
									free(serialCommPorts.third[j]);
									serialCommPorts.third[j] = (wchar_t*)malloc(descLength*sizeof(wchar_t));
									MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, devInfo[i].Description, -1, serialCommPorts.third[j], descLength);
									break;
								}
						}
					}
				}
				free(devInfo);
			}
		}
		FreeLibrary(ftdiLibInstance);
	}

	// Get relevant SerialComm methods and fill in com port array
	jobjectArray arrayObject = env->NewObjectArray(serialCommPorts.length, serialCommClass, 0);
	wchar_t systemPortName[128];
	int i;
	for (i = 0; i < serialCommPorts.length; ++i)
	{
		// Create new SerialComm object containing the enumerated values
		jobject serialCommObject = env->NewObject(serialCommClass, serialCommConstructor);
		wcscpy(systemPortName, L"\\\\.\\");
		wcscat(systemPortName, serialCommPorts.first[i]);
		env->SetObjectField(serialCommObject, comPortField, env->NewString((jchar*)systemPortName, wcslen(systemPortName)));
		env->SetObjectField(serialCommObject, friendlyNameField, env->NewString((jchar*)serialCommPorts.second[i], wcslen(serialCommPorts.second[i])));
		env->SetObjectField(serialCommObject, portDescriptionField, env->NewString((jchar*)serialCommPorts.third[i], wcslen(serialCommPorts.third[i])));
		free(serialCommPorts.first[i]);
		free(serialCommPorts.second[i]);
		free(serialCommPorts.third[i]);

		// Add new SerialComm object to array
		env->SetObjectArrayElement(arrayObject, i, serialCommObject);
	}
	free(serialCommPorts.first);
	free(serialCommPorts.second);
	free(serialCommPorts.third);
	return arrayObject;
}

JNIEXPORT void JNICALL Java_com_fazecast_jSerialComm_SerialPort_initializeLibrary(JNIEnv *env, jclass serialComm)
{
	// Cache class and method ID as global references
	serialCommClass = (jclass)env->NewGlobalRef(serialComm);
	serialCommConstructor = env->GetMethodID(serialCommClass, "<init>", "()V");

	// Cache
	serialPortHandleField = env->GetFieldID(serialCommClass, "portHandle", "J");
	comPortField = env->GetFieldID(serialCommClass, "comPort", "Ljava/lang/String;");
	friendlyNameField = env->GetFieldID(serialCommClass, "friendlyName", "Ljava/lang/String;");
	portDescriptionField = env->GetFieldID(serialCommClass, "portDescription", "Ljava/lang/String;");
	eventListenerRunningField = env->GetFieldID(serialCommClass, "eventListenerRunning", "Z");
	disableConfigField = env->GetFieldID(serialCommClass, "disableConfig", "Z");
	isDtrEnabledField = env->GetFieldID(serialCommClass, "isDtrEnabled", "Z");
	isRtsEnabledField = env->GetFieldID(serialCommClass, "isRtsEnabled", "Z");
	baudRateField = env->GetFieldID(serialCommClass, "baudRate", "I");
	dataBitsField = env->GetFieldID(serialCommClass, "dataBits", "I");
	stopBitsField = env->GetFieldID(serialCommClass, "stopBits", "I");
	parityField = env->GetFieldID(serialCommClass, "parity", "I");
	flowControlField = env->GetFieldID(serialCommClass, "flowControl", "I");
	sendDeviceQueueSizeField = env->GetFieldID(serialCommClass, "sendDeviceQueueSize", "I");
	receiveDeviceQueueSizeField = env->GetFieldID(serialCommClass, "receiveDeviceQueueSize", "I");
	rs485ModeField = env->GetFieldID(serialCommClass, "rs485Mode", "Z");
	rs485DelayBeforeField = env->GetFieldID(serialCommClass, "rs485DelayBefore", "I");
	rs485DelayAfterField = env->GetFieldID(serialCommClass, "rs485DelayAfter", "I");
	timeoutModeField = env->GetFieldID(serialCommClass, "timeoutMode", "I");
	readTimeoutField = env->GetFieldID(serialCommClass, "readTimeout", "I");
	writeTimeoutField = env->GetFieldID(serialCommClass, "writeTimeout", "I");
	eventFlagsField = env->GetFieldID(serialCommClass, "eventFlags", "I");
}

JNIEXPORT void JNICALL Java_com_fazecast_jSerialComm_SerialPort_uninitializeLibrary(JNIEnv *env, jclass serialComm)
{
	// Delete the cache global reference
	env->DeleteGlobalRef(serialCommClass);
}

JNIEXPORT jlong JNICALL Java_com_fazecast_jSerialComm_SerialPort_openPortNative(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)env->GetObjectField(obj, comPortField);
	const char *portName = env->GetStringUTFChars(portNameJString, NULL);

	// Try to open existing serial port with read/write access
	HANDLE serialPortHandle = INVALID_HANDLE_VALUE;
	if ((serialPortHandle = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED, NULL)) != INVALID_HANDLE_VALUE)
	{
		// Configure the port parameters and timeouts
		if (Java_com_fazecast_jSerialComm_SerialPort_configPort(env, obj, (jlong)serialPortHandle))
			env->SetLongField(obj, serialPortHandleField, (jlong)serialPortHandle);
		else
		{
			// Close the port if there was a problem setting the parameters
			int numRetries = 10;
			PurgeComm(serialPortHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
			while (!CloseHandle(serialPortHandle) && (numRetries-- > 0));
			serialPortHandle = INVALID_HANDLE_VALUE;
			env->SetLongField(obj, serialPortHandleField, -1l);
		}
	}

	env->ReleaseStringUTFChars(portNameJString, portName);
	return (jlong)serialPortHandle;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configPort(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	DCB dcbSerialParams{};
	dcbSerialParams.DCBlength = sizeof(DCB);

	// Get port parameters from Java class
	DWORD baudRate = (DWORD)env->GetIntField(obj, baudRateField);
	BYTE byteSize = (BYTE)env->GetIntField(obj, dataBitsField);
	int stopBitsInt = env->GetIntField(obj, stopBitsField);
	int parityInt = env->GetIntField(obj, parityField);
	int flowControl = env->GetIntField(obj, flowControlField);
	DWORD sendDeviceQueueSize = (DWORD)env->GetIntField(obj, sendDeviceQueueSizeField);
	DWORD receiveDeviceQueueSize = (DWORD)env->GetIntField(obj, receiveDeviceQueueSizeField);
	BYTE configDisabled = (BYTE)env->GetBooleanField(obj, disableConfigField);
	BYTE rs485ModeEnabled = (BYTE)env->GetBooleanField(obj, rs485ModeField);
	BYTE isDtrEnabled = env->GetBooleanField(obj, isDtrEnabledField);
	BYTE isRtsEnabled = env->GetBooleanField(obj, isRtsEnabledField);
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
	if (!configDisabled && (!SetupComm(serialPortHandle, receiveDeviceQueueSize, sendDeviceQueueSize) || !GetCommState(serialPortHandle, &dcbSerialParams)))
		return JNI_FALSE;

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
	dcbSerialParams.fAbortOnError = FALSE;
	dcbSerialParams.XonLim = 2048;
	dcbSerialParams.XoffLim = 512;
	dcbSerialParams.XonChar = (char)17;
	dcbSerialParams.XoffChar = (char)19;

	// Apply changes
	return (configDisabled ? Java_com_fazecast_jSerialComm_SerialPort_configEventFlags(env, obj, serialPortFD) :
			(SetCommState(serialPortHandle, &dcbSerialParams) && Java_com_fazecast_jSerialComm_SerialPort_configEventFlags(env, obj, serialPortFD)));
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configTimeouts(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	// Get port timeouts from Java class
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	COMMTIMEOUTS timeouts{};
	int timeoutMode = env->GetIntField(obj, timeoutModeField);
	DWORD readTimeout = (DWORD)env->GetIntField(obj, readTimeoutField);
	DWORD writeTimeout = (DWORD)env->GetIntField(obj, writeTimeoutField);

	// Set updated port timeouts
	timeouts.WriteTotalTimeoutMultiplier = 0;
	switch (timeoutMode)
	{
		case com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_SEMI_BLOCKING:		// Read Semi-blocking
		case (com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_SEMI_BLOCKING | com_fazecast_jSerialComm_SerialPort_TIMEOUT_WRITE_BLOCKING):		// Read Semi-blocking/Write Blocking
			timeouts.ReadIntervalTimeout = MAXDWORD;
			timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
			timeouts.ReadTotalTimeoutConstant = readTimeout ? readTimeout : 0x0FFFFFFF;
			timeouts.WriteTotalTimeoutConstant = writeTimeout;
			break;
		case com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING:		// Read Blocking
		case (com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING | com_fazecast_jSerialComm_SerialPort_TIMEOUT_WRITE_BLOCKING):		// Read/Write Blocking
			timeouts.ReadIntervalTimeout = 0;
			timeouts.ReadTotalTimeoutMultiplier = 0;
			timeouts.ReadTotalTimeoutConstant = readTimeout;
			timeouts.WriteTotalTimeoutConstant = writeTimeout;
			break;
		case com_fazecast_jSerialComm_SerialPort_TIMEOUT_SCANNER:			// Scanner Mode
			timeouts.ReadIntervalTimeout = MAXDWORD;
			timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
			timeouts.ReadTotalTimeoutConstant = 0x0FFFFFFF;
			timeouts.WriteTotalTimeoutConstant = writeTimeout;
			break;
		case com_fazecast_jSerialComm_SerialPort_TIMEOUT_NONBLOCKING:		// Read Non-blocking
		case (com_fazecast_jSerialComm_SerialPort_TIMEOUT_NONBLOCKING | com_fazecast_jSerialComm_SerialPort_TIMEOUT_WRITE_BLOCKING):		// Read Non-blocking/Write Blocking
		default:
			timeouts.ReadIntervalTimeout = MAXDWORD;
			timeouts.ReadTotalTimeoutMultiplier = 0;
			timeouts.ReadTotalTimeoutConstant = 0;
			timeouts.WriteTotalTimeoutConstant = writeTimeout;
			break;
	}

	// Apply changes
	return SetCommTimeouts(serialPortHandle, &timeouts);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configEventFlags(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;

	// Get event flags from Java class
	int eventsToMonitor = env->GetIntField(obj, eventFlagsField);
	int eventFlags = EV_ERR;
	if (((eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE) > 0) ||
			((eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED) > 0))
		eventFlags |= EV_RXCHAR;
	if ((eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_WRITTEN) > 0)
		eventFlags |= EV_TXEMPTY;

	// Change read timeouts if we are monitoring data received
	if ((eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED) > 0)
	{
		COMMTIMEOUTS timeouts{};
		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
		timeouts.ReadTotalTimeoutConstant = 1000;
		timeouts.WriteTotalTimeoutMultiplier = 0;
		timeouts.WriteTotalTimeoutConstant = 0;
		SetCommTimeouts(serialPortHandle, &timeouts);
	}
	else
		Java_com_fazecast_jSerialComm_SerialPort_configTimeouts(env, obj, serialPortFD);

	// Apply changes
	return SetCommMask(serialPortHandle, eventFlags);
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_waitForEvent(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return 0;
	OVERLAPPED overlappedStruct{};
	overlappedStruct.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (overlappedStruct.hEvent == NULL)
	{
		CloseHandle(overlappedStruct.hEvent);
		return 0;
	}

	// Wait for a serial port event
	BOOL listenerRunning = TRUE;
	DWORD eventMask = 0, numBytesRead = 0, errorsMask, readResult = WAIT_FAILED;
	if (WaitCommEvent(serialPortHandle, &eventMask, &overlappedStruct) == FALSE)
	{
		if (GetLastError() != ERROR_IO_PENDING)			// Problem occurred
		{
			// Problem reading, close port
			Java_com_fazecast_jSerialComm_SerialPort_closePortNative(env, obj, serialPortFD);
			listenerRunning = FALSE;
		}
		else
		{
			do
			{
				listenerRunning = env->GetBooleanField(obj, eventListenerRunningField);
				if (listenerRunning)
					readResult = WaitForSingleObject(overlappedStruct.hEvent, 500);
				else
				{
					CancelIo(serialPortHandle);
					readResult = WaitForSingleObject(overlappedStruct.hEvent, INFINITE);
				}
			} while ((readResult == WAIT_TIMEOUT) && listenerRunning);
			if ((readResult != WAIT_OBJECT_0) || (GetOverlappedResult(serialPortHandle, &overlappedStruct, &numBytesRead, FALSE) == FALSE))
				numBytesRead = 0;
		}
	}

	// Ensure that new data actually was received
	if (listenerRunning)
	{
		COMSTAT commInfo;
		if (!ClearCommError(serialPortHandle, &errorsMask, &commInfo))
			Java_com_fazecast_jSerialComm_SerialPort_closePortNative(env, obj, serialPortFD);
		else
			numBytesRead = commInfo.cbInQue;
	}

	// Return type of event if successful
	CloseHandle(overlappedStruct.hEvent);
	return (((eventMask & EV_RXCHAR) > 0) && numBytesRead) ? com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE :
			(((eventMask & EV_TXEMPTY) > 0) ? com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_WRITTEN : 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_closePortNative(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	// Purge any outstanding port operations
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_TRUE;
	PurgeComm(serialPortHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);

	// Force the port to enter non-blocking mode to ensure that any current reads return
	COMMTIMEOUTS timeouts{};
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.ReadIntervalTimeout = MAXDWORD;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 0;
	timeouts.WriteTotalTimeoutConstant = 0;
	SetCommTimeouts(serialPortHandle, &timeouts);

	// Close the port
	int numRetries = 10;
	while (!CloseHandle(serialPortHandle) && (GetLastError() != ERROR_INVALID_HANDLE) && (numRetries-- > 0));
	if (numRetries > 0)
	{
		env->SetLongField(obj, serialPortHandleField, -1l);
		return JNI_TRUE;
	}

	// Error closing port, reconfigure port settings
	Java_com_fazecast_jSerialComm_SerialPort_configEventFlags(env, obj, serialPortFD);
	return JNI_FALSE;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAvailable(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return -1;

	// Retrieve bytes available to read
	COMSTAT commInfo;
	DWORD errorsMask;
	if (!ClearCommError(serialPortHandle, &errorsMask, &commInfo))
	{
		// Problem detected, close port
		Java_com_fazecast_jSerialComm_SerialPort_closePortNative(env, obj, serialPortFD);
		serialPortHandle = INVALID_HANDLE_VALUE;
	}
	return (serialPortHandle == INVALID_HANDLE_VALUE) ? -1 : commInfo.cbInQue;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAwaitingWrite(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return -1;

	// Retrieve bytes awaiting write
	COMSTAT commInfo;
	DWORD errorsMask;
	if (!ClearCommError(serialPortHandle, &errorsMask, &commInfo))
	{
		// Problem detected, close port
		Java_com_fazecast_jSerialComm_SerialPort_closePortNative(env, obj, serialPortFD);
		serialPortHandle = INVALID_HANDLE_VALUE;
	}
	return (serialPortHandle == INVALID_HANDLE_VALUE) ? -1 : commInfo.cbOutQue;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_readBytes(JNIEnv *env, jobject obj, jlong serialPortFD, jbyteArray buffer, jlong bytesToRead, jlong offset)
{
	// Retrieve the serial port handle
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return -1;

	// Check for a disconnected serial port
	COMSTAT commInfo;
	DWORD errorsMask;
	if (!ClearCommError(serialPortHandle, &errorsMask, &commInfo))
	{
		// Problem detected, close port
		Java_com_fazecast_jSerialComm_SerialPort_closePortNative(env, obj, serialPortFD);
		return -1;
	}

	// Create an asynchronous result structure
	OVERLAPPED overlappedStruct{};
    overlappedStruct.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlappedStruct.hEvent == NULL)
    {
    	CloseHandle(overlappedStruct.hEvent);
    	return -1;
    }
    char *readBuffer = (char*)malloc(bytesToRead);
    DWORD numBytesRead = 0;

    // Read from serial port and close upon error
    BOOL result;
    if (((result = ReadFile(serialPortHandle, readBuffer, bytesToRead, NULL, &overlappedStruct)) == FALSE) && (GetLastError() != ERROR_IO_PENDING))
		Java_com_fazecast_jSerialComm_SerialPort_closePortNative(env, obj, serialPortFD);
	else if ((result = GetOverlappedResult(serialPortHandle, &overlappedStruct, &numBytesRead, TRUE)) == FALSE)
		Java_com_fazecast_jSerialComm_SerialPort_closePortNative(env, obj, serialPortFD);

    // Return number of bytes read if successful
    CloseHandle(overlappedStruct.hEvent);
    env->SetByteArrayRegion(buffer, offset, numBytesRead, (jbyte*)readBuffer);
    free(readBuffer);
	return (result == TRUE) ? numBytesRead : -1;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_writeBytes(JNIEnv *env, jobject obj, jlong serialPortFD, jbyteArray buffer, jlong bytesToWrite, jlong offset)
{
	// Retrieve the serial port handle
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return -1;

	// Check for a disconnected serial port
	COMSTAT commInfo;
	DWORD errorsMask;
	if (!ClearCommError(serialPortHandle, &errorsMask, &commInfo))
	{
		// Problem detected, close port
		Java_com_fazecast_jSerialComm_SerialPort_closePortNative(env, obj, serialPortFD);
		return -1;
	}

	// Create an asynchronous result structure
	OVERLAPPED overlappedStruct{};
	overlappedStruct.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (overlappedStruct.hEvent == NULL)
	{
		CloseHandle(overlappedStruct.hEvent);
		return -1;
	}
	jbyte *writeBuffer = env->GetByteArrayElements(buffer, 0);
	DWORD numBytesWritten = 0;
	BOOL result;

	// Write to serial port and close upon error
	if (((result = WriteFile(serialPortHandle, writeBuffer+offset, bytesToWrite, NULL, &overlappedStruct)) == FALSE) && (GetLastError() != ERROR_IO_PENDING))
		Java_com_fazecast_jSerialComm_SerialPort_closePortNative(env, obj, serialPortFD);
	else if ((result = GetOverlappedResult(serialPortHandle, &overlappedStruct, &numBytesWritten, TRUE)) == FALSE)
		Java_com_fazecast_jSerialComm_SerialPort_closePortNative(env, obj, serialPortFD);

	// Return number of bytes written if successful
	CloseHandle(overlappedStruct.hEvent);
	env->ReleaseByteArrayElements(buffer, writeBuffer, JNI_ABORT);
	return (result == TRUE) ? numBytesWritten : -1;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_setBreak(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	return SetCommBreak(serialPortHandle);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_clearBreak(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	return ClearCommBreak(serialPortHandle);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_setRTS(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	return EscapeCommFunction(serialPortHandle, SETRTS);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_clearRTS(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	return EscapeCommFunction(serialPortHandle, CLRRTS);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_presetRTS(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)env->GetObjectField(obj, comPortField);
	const char *portName = env->GetStringUTFChars(portNameJString, NULL);
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

	env->ReleaseStringUTFChars(portNameJString, portName);
	return (result != 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_preclearRTS(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)env->GetObjectField(obj, comPortField);
	const char *portName = env->GetStringUTFChars(portNameJString, NULL);
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

	env->ReleaseStringUTFChars(portNameJString, portName);
	return (result != 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_setDTR(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	return EscapeCommFunction(serialPortHandle, SETDTR);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_clearDTR(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	return EscapeCommFunction(serialPortHandle, CLRDTR);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_presetDTR(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)env->GetObjectField(obj, comPortField);
	const char *portName = env->GetStringUTFChars(portNameJString, NULL);
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

	env->ReleaseStringUTFChars(portNameJString, portName);
	return (result != 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_preclearDTR(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)env->GetObjectField(obj, comPortField);
	const char *portName = env->GetStringUTFChars(portNameJString, NULL);
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

	env->ReleaseStringUTFChars(portNameJString, portName);
	return (result != 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getCTS(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	DWORD modemStatus = 0;
	return GetCommModemStatus(serialPortHandle, &modemStatus) && (modemStatus & MS_CTS_ON);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getDSR(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	DWORD modemStatus = 0;
	return GetCommModemStatus(serialPortHandle, &modemStatus) && (modemStatus & MS_DSR_ON);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getDCD(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	DWORD modemStatus = 0;
	return GetCommModemStatus(serialPortHandle, &modemStatus) && (modemStatus & MS_RLSD_ON);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getDTR(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	return env->GetBooleanField(obj, isDtrEnabledField);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getRTS(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	return env->GetBooleanField(obj, isRtsEnabledField);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getRI(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	DWORD modemStatus = 0;
	return GetCommModemStatus(serialPortHandle, &modemStatus) && (modemStatus & MS_RING_ON);
}

#endif
