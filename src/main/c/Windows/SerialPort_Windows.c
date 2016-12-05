/*
 * SerialPort_Windows.c
 *
 *       Created on:  Feb 25, 2012
 *  Last Updated on:  Dec 05, 2016
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

#ifdef _WIN32
#define WINVER _WIN32_WINNT_WINXP
#define _WIN32_WINNT _WIN32_WINNT_WINXP
#define NTDDI_VERSION NTDDI_WINXP
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "../com_fazecast_jSerialComm_SerialPort.h"
#include "WindowsHelperFunctions.h"

// Cached class, method, and field IDs
jclass serialCommClass;
jmethodID serialCommConstructor;
jfieldID serialPortHandleField;
jfieldID comPortField;
jfieldID portStringField;
jfieldID isOpenedField;
jfieldID baudRateField;
jfieldID dataBitsField;
jfieldID stopBitsField;
jfieldID parityField;
jfieldID flowControlField;
jfieldID timeoutModeField;
jfieldID readTimeoutField;
jfieldID writeTimeoutField;
jfieldID eventFlagsField;

JNIEXPORT jobjectArray JNICALL Java_com_fazecast_jSerialComm_SerialPort_getCommPorts(JNIEnv *env, jclass serialComm)
{
	HKEY keyHandle1, keyHandle2, keyHandle3, keyHandle4, keyHandle5;
	DWORD numSubKeys1, numSubKeys2, numSubKeys3, numValues;
	DWORD maxSubKeyLength1, maxSubKeyLength2, maxSubKeyLength3;
	DWORD maxValueLength, maxComPortLength, valueLength, comPortLength, keyType;
	DWORD subKeyLength1, subKeyLength2, subKeyLength3, friendlyNameLength;

	// Enumerate serial ports on machine
	charPairVector serialCommPorts = { (char**)malloc(1), (char**)malloc(1), 0 };
	if ((RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_QUERY_VALUE, &keyHandle1) == ERROR_SUCCESS) &&
			(RegQueryInfoKey(keyHandle1, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, &maxValueLength, &maxComPortLength, NULL, NULL) == ERROR_SUCCESS))
	{
		// Allocate memory
		++maxValueLength;
		++maxComPortLength;
		CHAR *valueName = (CHAR*)malloc(maxValueLength);
		CHAR *comPort = (CHAR*)malloc(maxComPortLength);

		// Iterate through all COM ports
		for (DWORD i = 0; i < numValues; ++i)
		{
			// Get serial port name and COM value
			valueLength = maxValueLength;
			comPortLength = maxComPortLength;
			memset(valueName, 0, valueLength);
			memset(comPort, 0, comPortLength);
			if ((RegEnumValue(keyHandle1, i, valueName, &valueLength, NULL, &keyType, (BYTE*)comPort, &comPortLength) == ERROR_SUCCESS) && (keyType == REG_SZ))
			{
				// Set port name and description
				char* comPortString = (comPort[0] == '\\') ? (strrchr(comPort, '\\') + 1) : comPort;
				char* descriptionString = strrchr(valueName, '\\') ? (strrchr(valueName, '\\') + 1) : valueName;

				// Add new SerialComm object to vector
				push_back(&serialCommPorts, comPortString, descriptionString);
			}
		}

		// Clean up memory
		free(valueName);
		free(comPort);
	}
	RegCloseKey(keyHandle1);

	// Enumerate all devices on machine
	if ((RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Enum", 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &keyHandle1) == ERROR_SUCCESS) &&
			(RegQueryInfoKey(keyHandle1, NULL, NULL, NULL, &numSubKeys1, &maxSubKeyLength1, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS))
	{
		// Allocate memory
		++maxSubKeyLength1;
		CHAR *subKeyName1 = (CHAR*)malloc(maxSubKeyLength1);

		// Enumerate sub-keys
		for (DWORD i1 = 0; i1 < numSubKeys1; ++i1)
		{
			subKeyLength1 = maxSubKeyLength1;
			if ((RegEnumKeyEx(keyHandle1, i1, subKeyName1, &subKeyLength1, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) &&
					(RegOpenKeyEx(keyHandle1, subKeyName1, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &keyHandle2) == ERROR_SUCCESS) &&
					(RegQueryInfoKey(keyHandle2, NULL, NULL, NULL, &numSubKeys2, &maxSubKeyLength2, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS))
			{
				// Allocate memory
				++maxSubKeyLength2;
				CHAR *subKeyName2 = (CHAR*)malloc(maxSubKeyLength2);

				// Enumerate sub-keys
				for (DWORD i2 = 0; i2 < numSubKeys2; ++i2)
				{
					subKeyLength2 = maxSubKeyLength2;
					if ((RegEnumKeyEx(keyHandle2, i2, subKeyName2, &subKeyLength2, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) &&
							(RegOpenKeyEx(keyHandle2, subKeyName2, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &keyHandle3) == ERROR_SUCCESS) &&
							(RegQueryInfoKey(keyHandle3, NULL, NULL, NULL, &numSubKeys3, &maxSubKeyLength3, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS))
					{
						// Allocate memory
						++maxSubKeyLength3;
						CHAR *subKeyName3 = (CHAR*)malloc(maxSubKeyLength3);

						// Enumerate sub-keys
						for (DWORD i3 = 0; i3 < numSubKeys3; ++i3)
						{
							subKeyLength3 = maxSubKeyLength3;
							if ((RegEnumKeyEx(keyHandle3, i3, subKeyName3, &subKeyLength3, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) &&
									(RegOpenKeyEx(keyHandle3, subKeyName3, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &keyHandle4) == ERROR_SUCCESS) &&
									(RegQueryInfoKey(keyHandle4, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, NULL, &valueLength, NULL, NULL) == ERROR_SUCCESS))
							{
								// Allocate memory
								friendlyNameLength = valueLength + 1;
								CHAR *friendlyName = (CHAR*)malloc(friendlyNameLength);

								if ((RegOpenKeyEx(keyHandle4, "Device Parameters", 0, KEY_QUERY_VALUE, &keyHandle5) == ERROR_SUCCESS) &&
									(RegQueryInfoKey(keyHandle5, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, NULL, &valueLength, NULL, NULL) == ERROR_SUCCESS))
								{
									// Allocate memory
									comPortLength = valueLength + 1;
									CHAR *comPort = (CHAR*)malloc(comPortLength);

									// Attempt to get COM value and friendly port name
									if ((RegQueryValueEx(keyHandle5, "PortName", NULL, &keyType, (BYTE*)comPort, &comPortLength) == ERROR_SUCCESS) && (keyType == REG_SZ) &&
											(RegQueryValueEx(keyHandle4, "FriendlyName", NULL, &keyType, (BYTE*)friendlyName, &friendlyNameLength) == ERROR_SUCCESS) && (keyType == REG_SZ))
									{
										// Set port name and description
										char* comPortString = (comPort[0] == '\\') ? (strrchr(comPort, '\\') + 1) : comPort;
										char* descriptionString = friendlyName;

										// Update friendly name if COM port is actually connected and present in the port list
										int i;
										for (i = 0; i < serialCommPorts.length; ++i)
											if (strcmp(serialCommPorts.first[i], comPortString) == 0)
											{
												free(serialCommPorts.second[i]);
												serialCommPorts.second[i] = (char*)malloc(strlen(descriptionString)+1);
												strcpy(serialCommPorts.second[i], descriptionString);
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

	// Get relevant SerialComm methods and fill in com port array
	jobjectArray arrayObject = env->NewObjectArray(serialCommPorts.length, serialCommClass, 0);
	char systemPortName[128];
	int i;
	for (i = 0; i < serialCommPorts.length; ++i)
	{
		// Create new SerialComm object containing the enumerated values
		jobject serialCommObject = env->NewObject(serialCommClass, serialCommConstructor);
		strcpy(systemPortName, "\\\\.\\");
		strcat(systemPortName, serialCommPorts.first[i]);
		env->SetObjectField(serialCommObject, comPortField, env->NewStringUTF(systemPortName));
		env->SetObjectField(serialCommObject, portStringField, env->NewStringUTF(serialCommPorts.second[i]));
		free(serialCommPorts.first[i]);
		free(serialCommPorts.second[i]);

		// Add new SerialComm object to array
		env->SetObjectArrayElement(arrayObject, i, serialCommObject);
	}
	free(serialCommPorts.first);
	free(serialCommPorts.second);
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
	portStringField = env->GetFieldID(serialCommClass, "portString", "Ljava/lang/String;");
	isOpenedField = env->GetFieldID(serialCommClass, "isOpened", "Z");
	baudRateField = env->GetFieldID(serialCommClass, "baudRate", "I");
	dataBitsField = env->GetFieldID(serialCommClass, "dataBits", "I");
	stopBitsField = env->GetFieldID(serialCommClass, "stopBits", "I");
	parityField = env->GetFieldID(serialCommClass, "parity", "I");
	flowControlField = env->GetFieldID(serialCommClass, "flowControl", "I");
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
		if (Java_com_fazecast_jSerialComm_SerialPort_configPort(env, obj, (jlong)serialPortHandle) &&
				Java_com_fazecast_jSerialComm_SerialPort_configEventFlags(env, obj, (jlong)serialPortHandle))
			env->SetBooleanField(obj, isOpenedField, JNI_TRUE);
		else
		{
			// Close the port if there was a problem setting the parameters
			PurgeComm(serialPortHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
			while (!CloseHandle(serialPortHandle));
			serialPortHandle = INVALID_HANDLE_VALUE;
			env->SetBooleanField(obj, isOpenedField, JNI_FALSE);
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
	DCB dcbSerialParams = {0};
	dcbSerialParams.DCBlength = sizeof(DCB);

	// Get port parameters from Java class
	DWORD baudRate = (DWORD)env->GetIntField(obj, baudRateField);
	BYTE byteSize = (BYTE)env->GetIntField(obj, dataBitsField);
	int stopBitsInt = env->GetIntField(obj, stopBitsField);
	int parityInt = env->GetIntField(obj, parityField);
	int flowControl = env->GetIntField(obj, flowControlField);
	BYTE stopBits = (stopBitsInt == com_fazecast_jSerialComm_SerialPort_ONE_STOP_BIT) ? ONESTOPBIT : (stopBitsInt == com_fazecast_jSerialComm_SerialPort_ONE_POINT_FIVE_STOP_BITS) ? ONE5STOPBITS : TWOSTOPBITS;
	BYTE parity = (parityInt == com_fazecast_jSerialComm_SerialPort_NO_PARITY) ? NOPARITY : (parityInt == com_fazecast_jSerialComm_SerialPort_ODD_PARITY) ? ODDPARITY : (parityInt == com_fazecast_jSerialComm_SerialPort_EVEN_PARITY) ? EVENPARITY : (parityInt == com_fazecast_jSerialComm_SerialPort_MARK_PARITY) ? MARKPARITY : SPACEPARITY;
	BOOL isParity = (parityInt == com_fazecast_jSerialComm_SerialPort_NO_PARITY) ? FALSE : TRUE;
	BOOL CTSEnabled = (((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_CTS_ENABLED) > 0) ||
			((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_RTS_ENABLED) > 0));
	BOOL DSREnabled = (((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_DSR_ENABLED) > 0) ||
			((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_DTR_ENABLED) > 0));
	BYTE DTRValue = ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_DTR_ENABLED) > 0) ? DTR_CONTROL_HANDSHAKE : DTR_CONTROL_ENABLE;
	BYTE RTSValue = ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_RTS_ENABLED) > 0) ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;
	BOOL XonXoffInEnabled = ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_XONXOFF_IN_ENABLED) > 0);
	BOOL XonXoffOutEnabled = ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_XONXOFF_OUT_ENABLED) > 0);

	// Retrieve existing port configuration
	if (!GetCommState(serialPortHandle, &dcbSerialParams))
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
	return SetCommState(serialPortHandle, &dcbSerialParams);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configTimeouts(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	// Get port timeouts from Java class
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_FALSE;
	COMMTIMEOUTS timeouts = {0};
	int timeoutMode = env->GetIntField(obj, timeoutModeField);
	DWORD readTimeout = (DWORD)env->GetIntField(obj, readTimeoutField);
	DWORD writeTimeout = (DWORD)env->GetIntField(obj, writeTimeoutField);

	// Set updated port timeouts
	timeouts.WriteTotalTimeoutMultiplier = 0;
	switch (timeoutMode)
	{
		case com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_SEMI_BLOCKING:		// Read Semi-blocking
			timeouts.ReadIntervalTimeout = MAXDWORD;
			timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
			timeouts.ReadTotalTimeoutConstant = readTimeout;
			timeouts.WriteTotalTimeoutConstant = writeTimeout;
			break;
		case (com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_SEMI_BLOCKING | com_fazecast_jSerialComm_SerialPort_TIMEOUT_WRITE_SEMI_BLOCKING):	// Read/Write Semi-blocking
			timeouts.ReadIntervalTimeout = MAXDWORD;
			timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
			timeouts.ReadTotalTimeoutConstant = readTimeout;
			timeouts.WriteTotalTimeoutConstant = writeTimeout;
			break;
		case (com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_SEMI_BLOCKING | com_fazecast_jSerialComm_SerialPort_TIMEOUT_WRITE_BLOCKING):		// Read Semi-blocking/Write Blocking
			timeouts.ReadIntervalTimeout = MAXDWORD;
			timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
			timeouts.ReadTotalTimeoutConstant = readTimeout;
			timeouts.WriteTotalTimeoutConstant = writeTimeout;
			break;
		case com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING:		// Read Blocking
			timeouts.ReadIntervalTimeout = 0;
			timeouts.ReadTotalTimeoutMultiplier = 0;
			timeouts.ReadTotalTimeoutConstant = readTimeout;
			timeouts.WriteTotalTimeoutConstant = writeTimeout;
			break;
		case (com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING | com_fazecast_jSerialComm_SerialPort_TIMEOUT_WRITE_SEMI_BLOCKING):	// Read Blocking/Write Semi-blocking
			timeouts.ReadIntervalTimeout = 0;
			timeouts.ReadTotalTimeoutMultiplier = 0;
			timeouts.ReadTotalTimeoutConstant = readTimeout;
			timeouts.WriteTotalTimeoutConstant = writeTimeout;
			break;
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
			timeouts.WriteTotalTimeoutConstant = 0;
			break;
		case com_fazecast_jSerialComm_SerialPort_TIMEOUT_NONBLOCKING:		// Non-blocking
		default:
			timeouts.ReadIntervalTimeout = MAXDWORD;
			timeouts.ReadTotalTimeoutMultiplier = 0;
			timeouts.ReadTotalTimeoutConstant = 0;
			timeouts.WriteTotalTimeoutConstant = 0;
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
	int eventFlags = 0;
	if (((eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE) > 0) ||
			((eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED) > 0))
		eventFlags |= EV_RXCHAR;
	if ((eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_WRITTEN) > 0)
		eventFlags |= EV_TXEMPTY;

	// Change read timeouts if we are monitoring data received
	if ((eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED) > 0)
	{
		COMMTIMEOUTS timeouts = {0};
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
	OVERLAPPED overlappedStruct = {0};
	overlappedStruct.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (overlappedStruct.hEvent == NULL)
	{
		CloseHandle(overlappedStruct.hEvent);
		return 0;
	}

	// Wait for a serial port event
	DWORD eventMask, numBytesRead, readResult = WAIT_FAILED;
	if (WaitCommEvent(serialPortHandle, &eventMask, &overlappedStruct) == FALSE)
	{
		if (GetLastError() != ERROR_IO_PENDING)			// Problem occurred
		{
			// Problem reading, close port
			PurgeComm(serialPortHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
			while (!CloseHandle(serialPortHandle));
			serialPortHandle = INVALID_HANDLE_VALUE;
			env->SetLongField(obj, serialPortHandleField, -1l);
			env->SetBooleanField(obj, isOpenedField, JNI_FALSE);
		}
		else
		{
			BOOL continueWaiting = TRUE;
			while (continueWaiting)
			{
				readResult = WaitForSingleObject(overlappedStruct.hEvent, 750);
				continueWaiting = ((readResult == WAIT_TIMEOUT) && (env->GetIntField(obj, eventFlagsField) != 0));
			}
			if ((readResult != WAIT_OBJECT_0) || (GetOverlappedResult(serialPortHandle, &overlappedStruct, &numBytesRead, TRUE) == FALSE))
				numBytesRead = 0;
		}
	}

	// Return type of event if successful
	CloseHandle(overlappedStruct.hEvent);
	return ((eventMask & EV_RXCHAR) > 0) ? com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE :
			(((eventMask & EV_TXEMPTY) > 0) ? com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_WRITTEN : 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_closePortNative(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	// Purge any outstanding port operations
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return JNI_TRUE;
	PurgeComm(serialPortHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);

	// Close port
	int numRetries = 10;
	while (!CloseHandle(serialPortHandle) && (numRetries-- > 0));
	if (numRetries > 0)
	{
		serialPortHandle = INVALID_HANDLE_VALUE;
		env->SetLongField(obj, serialPortHandleField, -1l);
		env->SetBooleanField(obj, isOpenedField, JNI_FALSE);
		return JNI_TRUE;
	}

	return JNI_FALSE;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAvailable(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return -1;

	COMSTAT commInfo;
	if (!ClearCommError(serialPortHandle, NULL, &commInfo))
		return -1;
	DWORD numBytesAvailable = commInfo.cbInQue;

	return (jint)numBytesAvailable;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAwaitingWrite(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return -1;

	COMSTAT commInfo;
	if (!ClearCommError(serialPortHandle, NULL, &commInfo))
		return -1;
	DWORD numBytesToWrite = commInfo.cbOutQue;

	return (jint)numBytesToWrite;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_readBytes(JNIEnv *env, jobject obj, jlong serialPortFD, jbyteArray buffer, jlong bytesToRead)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return -1;
	OVERLAPPED overlappedStruct = {0};
    overlappedStruct.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlappedStruct.hEvent == NULL)
    {
    	CloseHandle(overlappedStruct.hEvent);
    	return -1;
    }
    char *readBuffer = (char*)malloc(bytesToRead);
    DWORD numBytesRead = 0;
    BOOL result;

    // Read from serial port
    if ((result = ReadFile(serialPortHandle, readBuffer, bytesToRead, &numBytesRead, &overlappedStruct)) == FALSE)
    {
    	if (GetLastError() != ERROR_IO_PENDING)			// Problem occurred
		{
			// Problem reading, close port
			PurgeComm(serialPortHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
			while (!CloseHandle(serialPortHandle));
			serialPortHandle = INVALID_HANDLE_VALUE;
			env->SetLongField(obj, serialPortHandleField, -1l);
			env->SetBooleanField(obj, isOpenedField, JNI_FALSE);
		}
		else if ((result = GetOverlappedResult(serialPortHandle, &overlappedStruct, &numBytesRead, TRUE)) == FALSE)
		{
			// Problem reading, close port
			PurgeComm(serialPortHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
			while (!CloseHandle(serialPortHandle));
			serialPortHandle = INVALID_HANDLE_VALUE;
			env->SetLongField(obj, serialPortHandleField, -1l);
			env->SetBooleanField(obj, isOpenedField, JNI_FALSE);
		}
    }

    // Return number of bytes read if successful
    CloseHandle(overlappedStruct.hEvent);
    env->SetByteArrayRegion(buffer, 0, numBytesRead, (jbyte*)readBuffer);
    free(readBuffer);
	return (result == TRUE) ? numBytesRead : -1;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_writeBytes(JNIEnv *env, jobject obj, jlong serialPortFD, jbyteArray buffer, jlong bytesToWrite)
{
	HANDLE serialPortHandle = (HANDLE)serialPortFD;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		return -1;
	OVERLAPPED overlappedStruct = {0};
	overlappedStruct.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (overlappedStruct.hEvent == NULL)
	{
		CloseHandle(overlappedStruct.hEvent);
		return -1;
	}
	jbyte *writeBuffer = env->GetByteArrayElements(buffer, 0);
	DWORD numBytesWritten = 0;
	BOOL result;

	// Set the DTR line to high if using RS-422
	//EscapeCommFunction(serialPortHandle, SETDTR);

	// Write to serial port
	if ((result = WriteFile(serialPortHandle, writeBuffer, bytesToWrite, &numBytesWritten, &overlappedStruct)) == FALSE)
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			// Problem writing, close port
			PurgeComm(serialPortHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
			while (!CloseHandle(serialPortHandle));
			serialPortHandle = INVALID_HANDLE_VALUE;
			env->SetLongField(obj, serialPortHandleField, -1l);
			env->SetBooleanField(obj, isOpenedField, JNI_FALSE);
		}
		else if ((result = GetOverlappedResult(serialPortHandle, &overlappedStruct, &numBytesWritten, TRUE)) == FALSE)
		{
			// Problem reading, close port
			PurgeComm(serialPortHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
			while (!CloseHandle(serialPortHandle));
			serialPortHandle = INVALID_HANDLE_VALUE;
			env->SetLongField(obj, serialPortHandleField, -1l);
			env->SetBooleanField(obj, isOpenedField, JNI_FALSE);
		}
	}

	// Clear the DTR line if using RS-422
	//COMSTAT commInfo;
	//do { ClearCommError(serialPortHandle, NULL, &commInfo); } while (commInfo.cbOutQue > 0);
	//EscapeCommFunction(serialPortHandle, CLRDTR);

	// Return number of bytes written if successful
	CloseHandle(overlappedStruct.hEvent);
	env->ReleaseByteArrayElements(buffer, writeBuffer, JNI_ABORT);
	return (result == TRUE) ? numBytesWritten : -1;
}

#endif
