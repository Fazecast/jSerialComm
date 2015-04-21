/*
 * SerialPort_Android.cpp
 *
 *       Created on:  Mar 13, 2015
 *  Last Updated on:  Mar 13, 2015
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2015 Fazecast, Inc.
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
#ifndef CMSPAR
#define CMSPAR 010000000000
#endif
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include "com_fazecast_jSerialComm_SerialPort.h"
#include "AndroidHelperFunctions.h"

JNIEXPORT jobjectArray JNICALL Java_com_fazecast_jSerialComm_SerialPort_getCommPorts(JNIEnv *env, jclass serialCommClass)
{
	// Get relevant SerialComm methods and IDs
	jmethodID serialCommConstructor = (*env)->GetMethodID(env, serialCommClass, "<init>", "()V");
	jfieldID portStringID = (*env)->GetFieldID(env, serialCommClass, "portString", "Ljava/lang/String;");
	jfieldID comPortID = (*env)->GetFieldID(env, serialCommClass, "comPort", "Ljava/lang/String;");

	// Enumerate serial ports on machine
	charPairVector serialPorts = { (char**)malloc(1), (char**)malloc(1), 0 };
	recursiveSearchForComPorts(&serialPorts, "/sys/devices/");
	jobjectArray arrayObject = (*env)->NewObjectArray(env, serialPorts.length, serialCommClass, 0);
	int index = 0, i;
	for (i = 0; i < serialPorts.length; ++i)
	{
		// Create new SerialComm object containing the enumerated values
		jobject serialCommObject = (*env)->NewObject(env, serialCommClass, serialCommConstructor);
		(*env)->SetObjectField(env, serialCommObject, portStringID, (*env)->NewStringUTF(env, serialPorts.second[i]));
		(*env)->SetObjectField(env, serialCommObject, comPortID, (*env)->NewStringUTF(env, serialPorts.first[i]));
		free(serialPorts.first[i]);
		free(serialPorts.second[i]);

		// Add new SerialComm object to array
		(*env)->SetObjectArrayElement(env, arrayObject, index++, serialCommObject);
	}
	free(serialPorts.first);
	free(serialPorts.second);

	return arrayObject;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_openPortNative(JNIEnv *env, jobject obj)
{
	int fdSerial;
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "comPort", "Ljava/lang/String;"));
	const char *portName = (*env)->GetStringUTFChars(env, portNameJString, NULL);

	// Try to open existing serial port with read/write access
	if ((fdSerial = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK)) > 0)
	{
		// Set port handle in Java structure
		(*env)->SetLongField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "portHandle", "J"), fdSerial);

		// Configure the port parameters and timeouts
		if (Java_com_fazecast_jSerialComm_SerialPort_configPort(env, obj) && Java_com_fazecast_jSerialComm_SerialPort_configFlowControl(env, obj) &&
				Java_com_fazecast_jSerialComm_SerialPort_configEventFlags(env, obj))
			(*env)->SetBooleanField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "isOpened", "Z"), JNI_TRUE);
		else
		{
			// Close the port if there was a problem setting the parameters
			close(fdSerial);
			fdSerial = -1;
			(*env)->SetLongField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "portHandle", "J"), -1l);
			(*env)->SetBooleanField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "isOpened", "Z"), JNI_FALSE);
		}
	}

	(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
	return (fdSerial == -1) ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configPort(JNIEnv *env, jobject obj)
{
	struct termios options = { 0 };
	jclass serialCommClass = (*env)->GetObjectClass(env, obj);
	int portFD = (int)(*env)->GetLongField(env, obj, (*env)->GetFieldID(env, serialCommClass, "portHandle", "J"));
	if (portFD <= 0)
		return JNI_FALSE;

	// Get port parameters from Java class
	int baudRate = (*env)->GetIntField(env, obj, (*env)->GetFieldID(env, serialCommClass, "baudRate", "I"));
	int byteSizeInt = (*env)->GetIntField(env, obj, (*env)->GetFieldID(env, serialCommClass, "dataBits", "I"));
	int stopBitsInt = (*env)->GetIntField(env, obj, (*env)->GetFieldID(env, serialCommClass, "stopBits", "I"));
	int parityInt = (*env)->GetIntField(env, obj, (*env)->GetFieldID(env, serialCommClass, "parity", "I"));
	tcflag_t byteSize = (byteSizeInt == 5) ? CS5 : (byteSizeInt == 6) ? CS6 : (byteSizeInt == 7) ? CS7 : CS8;
	tcflag_t stopBits = ((stopBitsInt == com_fazecast_jSerialComm_SerialPort_ONE_STOP_BIT) || (stopBitsInt == com_fazecast_jSerialComm_SerialPort_ONE_POINT_FIVE_STOP_BITS)) ? 0 : CSTOPB;
	tcflag_t parity = (parityInt == com_fazecast_jSerialComm_SerialPort_NO_PARITY) ? 0 : (parityInt == com_fazecast_jSerialComm_SerialPort_ODD_PARITY) ? (PARENB | PARODD) : (parityInt == com_fazecast_jSerialComm_SerialPort_EVEN_PARITY) ? PARENB : (parityInt == com_fazecast_jSerialComm_SerialPort_MARK_PARITY) ? (PARENB | CMSPAR | PARODD) : (PARENB | CMSPAR);

	// Clear any serial port flags
	fcntl(portFD, F_SETFL, 0);

	// Set raw-mode to allow the use of tcsetattr() and ioctl()
	tcgetattr(portFD, &options);
	cfmakeraw(&options);

	// Set updated port parameters
	options.c_cflag = (byteSize | stopBits | parity | CLOCAL | CREAD);
	if (parityInt == com_fazecast_jSerialComm_SerialPort_SPACE_PARITY)
		options.c_cflag &= ~PARODD;
	options.c_iflag &= ~(INPCK | IGNPAR);
	options.c_iflag |= ((parityInt > 0) ? (INPCK | ISTRIP) : IGNPAR);

	// Set baud rate
	unsigned int baudRateCode = getBaudRateCode(baudRate);
	if (baudRateCode != 0)
	{
		cfsetispeed(&options, baudRateCode);
		cfsetospeed(&options, baudRateCode);
	}

	// Apply changes
	int retVal = tcsetattr(portFD, TCSANOW, &options);
	ioctl(portFD, TIOCEXCL);				// Block non-root users from using this port
	if (baudRateCode == 0)					// Set custom baud rate
		setBaudRate(portFD, baudRate);
	return ((retVal == 0) ? JNI_TRUE : JNI_FALSE);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configFlowControl(JNIEnv *env, jobject obj)
{
	struct termios options = { 0 };
	jclass serialCommClass = (*env)->GetObjectClass(env, obj);
	int portFD = (int)(*env)->GetLongField(env, obj, (*env)->GetFieldID(env, serialCommClass, "portHandle", "J"));
	if (portFD <= 0)
		return JNI_FALSE;

	// Get port parameters from Java class
	int baudRate = (*env)->GetIntField(env, obj, (*env)->GetFieldID(env, serialCommClass, "baudRate", "I"));
	unsigned int baudRateCode = getBaudRateCode(baudRate);
	int flowControl = (*env)->GetIntField(env, obj, (*env)->GetFieldID(env, serialCommClass, "flowControl", "I"));
	tcflag_t CTSRTSEnabled = (((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_CTS_ENABLED) > 0) ||
			((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_RTS_ENABLED) > 0)) ? CRTSCTS : 0;
	tcflag_t XonXoffInEnabled = ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_XONXOFF_IN_ENABLED) > 0) ? IXOFF : 0;
	tcflag_t XonXoffOutEnabled = ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_XONXOFF_OUT_ENABLED) > 0) ? IXON : 0;

	// Retrieve existing port configuration
	tcgetattr(portFD, &options);

	// Set updated port parameters
	options.c_cflag |= CTSRTSEnabled;
	options.c_iflag |= XonXoffInEnabled | XonXoffOutEnabled;

	// Apply changes
	int retVal = tcsetattr(portFD, TCSANOW, &options);
	if (baudRateCode == 0)					// Set custom baud rate
		setBaudRate(portFD, baudRate);
	return ((retVal == 0) ? JNI_TRUE : JNI_FALSE);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configTimeouts(JNIEnv *env, jobject obj)
{
	// Get port timeouts from Java class
	jclass serialCommClass = (*env)->GetObjectClass(env, obj);
	int serialFD = (int)(*env)->GetLongField(env, obj, (*env)->GetFieldID(env, serialCommClass, "portHandle", "J"));
	int baudRate = (*env)->GetIntField(env, obj, (*env)->GetFieldID(env, serialCommClass, "baudRate", "I"));
	unsigned int baudRateCode = getBaudRateCode(baudRate);
	int timeoutMode = (*env)->GetIntField(env, obj, (*env)->GetFieldID(env, serialCommClass, "timeoutMode", "I"));
	int readTimeout = (*env)->GetIntField(env, obj, (*env)->GetFieldID(env, serialCommClass, "readTimeout", "I"));
	if (serialFD <= 0)
		return JNI_FALSE;

	// Retrieve existing port configuration
	struct termios options = { 0 };
	tcgetattr(serialFD, &options);
	int flags = fcntl(serialFD, F_GETFL);

	// Set updated port timeouts
	if (((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_SEMI_BLOCKING) > 0) && (readTimeout > 0))	// Read Semi-blocking with timeout
	{
		flags &= ~O_NONBLOCK;
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = readTimeout / 100;
	}
	else if ((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_SEMI_BLOCKING) > 0)					// Read Semi-blocking without timeout
	{
		flags &= ~O_NONBLOCK;
		options.c_cc[VMIN] = 1;
		options.c_cc[VTIME] = 0;
	}
	else if (((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING) > 0)  && (readTimeout > 0))	// Read Blocking with timeout
	{
		flags &= ~O_NONBLOCK;
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = readTimeout / 100;
	}
	else if ((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING) > 0)							// Read Blocking without timeout
	{
		flags &= ~O_NONBLOCK;
		options.c_cc[VMIN] = 1;
		options.c_cc[VTIME] = 0;
	}
	else																											// Non-blocking
	{
		flags |= O_NONBLOCK;
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = 0;
	}

	// Apply changes
	fcntl(serialFD, F_SETFL, flags);
	int retVal = tcsetattr(serialFD, TCSANOW, &options);
	if (baudRateCode == 0)					// Set custom baud rate
		setBaudRate(serialFD, baudRate);
	return ((retVal == 0) ? JNI_TRUE : JNI_FALSE);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configEventFlags(JNIEnv *env, jobject obj)
{
	jclass serialCommClass = (*env)->GetObjectClass(env, obj);
	int serialFD = (int)(*env)->GetLongField(env, obj, (*env)->GetFieldID(env, serialCommClass, "portHandle", "J"));
	if (serialFD <= 0)
		return JNI_FALSE;

	// Get event flags from Java class
	int baudRate = (*env)->GetIntField(env, obj, (*env)->GetFieldID(env, serialCommClass, "baudRate", "I"));
	unsigned int baudRateCode = getBaudRateCode(baudRate);
	int eventsToMonitor = (*env)->GetIntField(env, obj, (*env)->GetFieldID(env, serialCommClass, "eventFlags", "I"));

	// Change read timeouts if we are monitoring data received
	if ((eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED) > 0)
	{
		struct termios options = { 0 };
		tcgetattr(serialFD, &options);
		int flags = fcntl(serialFD, F_GETFL);
		flags &= ~O_NONBLOCK;
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = 10;
		fcntl(serialFD, F_SETFL, flags);
		tcsetattr(serialFD, TCSANOW, &options);
		if (baudRateCode == 0)					// Set custom baud rate
			setBaudRate(serialFD, baudRate);
	}
	else
		Java_com_fazecast_jSerialComm_SerialPort_configTimeouts(env, obj);

	// Apply changes
	return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_waitForEvent(JNIEnv *env, jobject obj)
{
	jclass serialCommClass = (*env)->GetObjectClass(env, obj);
	int serialFD = (int)(*env)->GetLongField(env, obj, (*env)->GetFieldID(env, serialCommClass, "portHandle", "J"));
	if (serialFD <= 0)
		return 0;

	// Initialize the waiting set and the timeouts
	struct timeval timeout = { 1, 0 };
	fd_set waitingSet;
	FD_ZERO(&waitingSet);
	FD_SET(serialFD, &waitingSet);

	// Wait for a serial port event
	int retVal = select(serialFD + 1, &waitingSet, NULL, NULL, &timeout);
	if (retVal <= 0)
		return 0;
	return (FD_ISSET(serialFD, &waitingSet)) ? com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE : 0;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_closePortNative(JNIEnv *env, jobject obj)
{
	// Close port
	int portFD = (int)(*env)->GetLongField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "portHandle", "J"));
	if (portFD <= 0)
		return JNI_TRUE;
	close(portFD);
	(*env)->SetLongField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "portHandle", "J"), -1l);
	(*env)->SetBooleanField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "isOpened", "Z"), JNI_FALSE);

	return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAvailable(JNIEnv *env, jobject obj)
{
	int serialPortFD = (int)(*env)->GetLongField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "portHandle", "J"));
	int numBytesAvailable = -1;

	if (serialPortFD > 0)
		ioctl(serialPortFD, FIONREAD, &numBytesAvailable);

	return numBytesAvailable;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_readBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToRead)
{
	// Get port handle and read timeout from Java class
	jclass serialCommClass = (*env)->GetObjectClass(env, obj);
	int timeoutMode = (*env)->GetIntField(env, obj, (*env)->GetFieldID(env, serialCommClass, "timeoutMode", "I"));
	int readTimeout = (*env)->GetIntField(env, obj, (*env)->GetFieldID(env, serialCommClass, "readTimeout", "I"));
	int serialPortFD = (int)(*env)->GetLongField(env, obj, (*env)->GetFieldID(env, serialCommClass, "portHandle", "J"));
	if (serialPortFD == -1)
		return -1;
	int numBytesRead, numBytesReadTotal = 0, bytesRemaining = bytesToRead;
	char* readBuffer = (char*)malloc(bytesToRead);

	// Infinite blocking mode specified, don't return until we have completely finished the read
	if (((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING) > 0) && (readTimeout == 0))
	{
		// While there are more bytes we are supposed to read
		while (bytesRemaining > 0)
		{
			if ((numBytesRead = read(serialPortFD, readBuffer+numBytesReadTotal, bytesRemaining)) == -1)
			{
				// Problem reading, close port
				close(serialPortFD);
				(*env)->SetLongField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "portHandle", "J"), -1l);
				(*env)->SetBooleanField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "isOpened", "Z"), JNI_FALSE);
				break;
			}

			// Fix index variables
			numBytesReadTotal += numBytesRead;
			bytesRemaining -= numBytesRead;
		}
	}
	else if ((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING) > 0)		// Blocking mode, but not indefinitely
	{
		// Get current system time
		struct timeval expireTime = { 0 }, currTime = { 0 };
		gettimeofday(&expireTime, NULL);
		expireTime.tv_usec += (readTimeout * 1000);
		if (expireTime.tv_usec > 1000000)
		{
			expireTime.tv_sec += (expireTime.tv_usec * 0.000001);
			expireTime.tv_usec = (expireTime.tv_usec % 1000000);
		}

		// While there are more bytes we are supposed to read and the timeout has not elapsed
		do
		{
			if ((numBytesRead = read(serialPortFD, readBuffer+numBytesReadTotal, bytesRemaining)) == -1)
			{
				// Problem reading, close port
				close(serialPortFD);
				(*env)->SetLongField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "portHandle", "J"), -1l);
				(*env)->SetBooleanField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "isOpened", "Z"), JNI_FALSE);
				break;
			}

			// Fix index variables
			numBytesReadTotal += numBytesRead;
			bytesRemaining -= numBytesRead;

			// Get current system time
			gettimeofday(&currTime, NULL);
		} while ((bytesRemaining > 0) && ((expireTime.tv_sec > currTime.tv_sec) ||
				((expireTime.tv_sec == currTime.tv_sec) && (expireTime.tv_usec > currTime.tv_usec))));
	}
	else		// Semi- or non-blocking specified
	{
		// Read from port
		if ((numBytesRead = read(serialPortFD, readBuffer, bytesToRead)) == -1)
		{
			// Problem reading, close port
			close(serialPortFD);
			(*env)->SetLongField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "portHandle", "J"), -1l);
			(*env)->SetBooleanField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "isOpened", "Z"), JNI_FALSE);
		}
		else
			numBytesReadTotal = numBytesRead;
	}

	// Return number of bytes read if successful
	(*env)->SetByteArrayRegion(env, buffer, 0, numBytesReadTotal, (jbyte*)readBuffer);
	free(readBuffer);
	return (numBytesRead == -1) ? -1 : numBytesReadTotal;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_writeBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToWrite)
{
	int serialPortFD = (int)(*env)->GetLongField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "portHandle", "J"));
	if (serialPortFD == -1)
		return -1;
	jbyte *writeBuffer = (*env)->GetByteArrayElements(env, buffer, 0);
	int numBytesWritten;

	// Write to port
	if ((numBytesWritten = write(serialPortFD, writeBuffer, bytesToWrite)) == -1)
	{
		// Problem writing, close port
		close(serialPortFD);
		(*env)->SetLongField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "portHandle", "J"), -1l);
		(*env)->SetBooleanField(env, obj, (*env)->GetFieldID(env, (*env)->GetObjectClass(env, obj), "isOpened", "Z"), JNI_FALSE);
	}

	// Return number of bytes written if successful
	(*env)->ReleaseByteArrayElements(env, buffer, writeBuffer, JNI_ABORT);
	return numBytesWritten;
}

#endif
