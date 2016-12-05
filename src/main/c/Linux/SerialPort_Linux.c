/*
 * SerialPort_Linux.c
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

#ifdef __linux__
#ifndef CMSPAR
#define CMSPAR 010000000000
#endif
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include "../com_fazecast_jSerialComm_SerialPort.h"
#include "LinuxHelperFunctions.h"

// Cached class, method, and field IDs
jclass serialCommClass;
jmethodID serialCommConstructor;
jfieldID serialPortFdField;
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
	// Enumerate serial ports on machine
	charPairVector serialPorts = { (char**)malloc(1), (char**)malloc(1), 0 };
	recursiveSearchForComPorts(&serialPorts, "/sys/devices/");
	lastDitchSearchForComPorts(&serialPorts);
	jobjectArray arrayObject = (*env)->NewObjectArray(env, serialPorts.length, serialCommClass, 0);
	int i;
	for (i = 0; i < serialPorts.length; ++i)
	{
		// Create new SerialComm object containing the enumerated values
		jobject serialCommObject = (*env)->NewObject(env, serialCommClass, serialCommConstructor);
		(*env)->SetObjectField(env, serialCommObject, portStringField, (*env)->NewStringUTF(env, serialPorts.second[i]));
		(*env)->SetObjectField(env, serialCommObject, comPortField, (*env)->NewStringUTF(env, serialPorts.first[i]));
		free(serialPorts.first[i]);
		free(serialPorts.second[i]);

		// Add new SerialComm object to array
		(*env)->SetObjectArrayElement(env, arrayObject, i, serialCommObject);
	}
	free(serialPorts.first);
	free(serialPorts.second);

	return arrayObject;
}

JNIEXPORT void JNICALL Java_com_fazecast_jSerialComm_SerialPort_initializeLibrary(JNIEnv *env, jclass serialComm)
{
	// Cache class and method ID as global references
	serialCommClass = (jclass)(*env)->NewGlobalRef(env, serialComm);
	serialCommConstructor = (*env)->GetMethodID(env, serialCommClass, "<init>", "()V");

	// Cache
	serialPortFdField = (*env)->GetFieldID(env, serialCommClass, "portHandle", "J");
	comPortField = (*env)->GetFieldID(env, serialCommClass, "comPort", "Ljava/lang/String;");
	portStringField = (*env)->GetFieldID(env, serialCommClass, "portString", "Ljava/lang/String;");
	isOpenedField = (*env)->GetFieldID(env, serialCommClass, "isOpened", "Z");
	baudRateField = (*env)->GetFieldID(env, serialCommClass, "baudRate", "I");
	dataBitsField = (*env)->GetFieldID(env, serialCommClass, "dataBits", "I");
	stopBitsField = (*env)->GetFieldID(env, serialCommClass, "stopBits", "I");
	parityField = (*env)->GetFieldID(env, serialCommClass, "parity", "I");
	flowControlField = (*env)->GetFieldID(env, serialCommClass, "flowControl", "I");
	timeoutModeField = (*env)->GetFieldID(env, serialCommClass, "timeoutMode", "I");
	readTimeoutField = (*env)->GetFieldID(env, serialCommClass, "readTimeout", "I");
	writeTimeoutField = (*env)->GetFieldID(env, serialCommClass, "writeTimeout", "I");
	eventFlagsField = (*env)->GetFieldID(env, serialCommClass, "eventFlags", "I");
}

JNIEXPORT void JNICALL Java_com_fazecast_jSerialComm_SerialPort_uninitializeLibrary(JNIEnv *env, jclass serialComm)
{
	// Delete the cache global reference
	(*env)->DeleteGlobalRef(env, serialCommClass);
}

JNIEXPORT jlong JNICALL Java_com_fazecast_jSerialComm_SerialPort_openPortNative(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	const char *portName = (*env)->GetStringUTFChars(env, portNameJString, NULL);

	// Try to open existing serial port with read/write access
	int serialPortFD = -1;
	if ((serialPortFD = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK)) > 0)
	{
		// Clear any serial port flags and set up raw, non-canonical port parameters
		struct termios options = { 0 };
		fcntl(serialPortFD, F_SETFL, 0);
		tcgetattr(serialPortFD, &options);
		cfmakeraw(&options);
		tcsetattr(serialPortFD, TCSANOW, &options);

		// Configure the port parameters and timeouts
		if (Java_com_fazecast_jSerialComm_SerialPort_configPort(env, obj, serialPortFD) &&
				Java_com_fazecast_jSerialComm_SerialPort_configEventFlags(env, obj, serialPortFD))
			(*env)->SetBooleanField(env, obj, isOpenedField, JNI_TRUE);
		else
		{
			// Close the port if there was a problem setting the parameters
			ioctl(serialPortFD, TIOCNXCL);
			tcdrain(serialPortFD);
			while ((close(serialPortFD) == -1) && (errno != EBADF));
			serialPortFD = -1;
			(*env)->SetBooleanField(env, obj, isOpenedField, JNI_FALSE);
		}
	}

	(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
	return serialPortFD;
}

//JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_associateNativeHandle(JNIEnv *env, jobject obj, jlong serialPortFD)
//{
//	;
//}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configPort(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	if (serialPortFD <= 0)
		return JNI_FALSE;
	struct termios options = { 0 };

	// Get port parameters from Java class
	int baudRate = (*env)->GetIntField(env, obj, baudRateField);
	int byteSizeInt = (*env)->GetIntField(env, obj, dataBitsField);
	int stopBitsInt = (*env)->GetIntField(env, obj, stopBitsField);
	int parityInt = (*env)->GetIntField(env, obj, parityField);
	int flowControl = (*env)->GetIntField(env, obj, flowControlField);
	tcflag_t byteSize = (byteSizeInt == 5) ? CS5 : (byteSizeInt == 6) ? CS6 : (byteSizeInt == 7) ? CS7 : CS8;
	tcflag_t stopBits = ((stopBitsInt == com_fazecast_jSerialComm_SerialPort_ONE_STOP_BIT) || (stopBitsInt == com_fazecast_jSerialComm_SerialPort_ONE_POINT_FIVE_STOP_BITS)) ? 0 : CSTOPB;
	tcflag_t parity = (parityInt == com_fazecast_jSerialComm_SerialPort_NO_PARITY) ? 0 : (parityInt == com_fazecast_jSerialComm_SerialPort_ODD_PARITY) ? (PARENB | PARODD) : (parityInt == com_fazecast_jSerialComm_SerialPort_EVEN_PARITY) ? PARENB : (parityInt == com_fazecast_jSerialComm_SerialPort_MARK_PARITY) ? (PARENB | CMSPAR | PARODD) : (PARENB | CMSPAR);
	tcflag_t CTSRTSEnabled = (((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_CTS_ENABLED) > 0) ||
			((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_RTS_ENABLED) > 0)) ? CRTSCTS : 0;
	tcflag_t XonXoffInEnabled = ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_XONXOFF_IN_ENABLED) > 0) ? IXOFF : 0;
	tcflag_t XonXoffOutEnabled = ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_XONXOFF_OUT_ENABLED) > 0) ? IXON : 0;

	// Set updated port parameters
	tcgetattr(serialPortFD, &options);
	options.c_cflag = (byteSize | stopBits | parity | CLOCAL | CREAD | CTSRTSEnabled);
	if (parityInt == com_fazecast_jSerialComm_SerialPort_SPACE_PARITY)
		options.c_cflag &= ~PARODD;
	options.c_iflag &= ~(INPCK | IGNPAR | PARMRK | ISTRIP);
	if (byteSizeInt < 8)
		options.c_iflag |= ISTRIP;
	if (parityInt != 0)
		options.c_iflag |= (INPCK | IGNPAR);
	options.c_iflag |= (XonXoffInEnabled | XonXoffOutEnabled);

	// Set baud rate
	unsigned int baudRateCode = getBaudRateCode(baudRate);
	if (baudRateCode != 0)
	{
		cfsetispeed(&options, baudRateCode);
		cfsetospeed(&options, baudRateCode);
	}

	// Apply changes
	int retVal = tcsetattr(serialPortFD, TCSANOW, &options);
	ioctl(serialPortFD, TIOCEXCL);			// Block non-root users from opening this port
	if (baudRateCode == 0)					// Set custom baud rate
		setBaudRate(serialPortFD, baudRate);
	return ((retVal == 0) ? JNI_TRUE : JNI_FALSE);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configTimeouts(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	// Get port timeouts from Java class
	if (serialPortFD <= 0)
		return JNI_FALSE;
	int baudRate = (*env)->GetIntField(env, obj, baudRateField);
	unsigned int baudRateCode = getBaudRateCode(baudRate);
	int timeoutMode = (*env)->GetIntField(env, obj, timeoutModeField);
	int readTimeout = (*env)->GetIntField(env, obj, readTimeoutField);

	// Retrieve existing port configuration
	struct termios options = { 0 };
	tcgetattr(serialPortFD, &options);
	int flags = fcntl(serialPortFD, F_GETFL);
	if (flags == -1)
		return JNI_FALSE;

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
	else if ((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_SCANNER) > 0)								// Scanner Mode
	{
		flags &= ~O_NONBLOCK;
		options.c_cc[VMIN] = 1;
		options.c_cc[VTIME] = 1;
	}
	else																											// Non-blocking
	{
		flags |= O_NONBLOCK;
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = 0;
	}

	// Apply changes
	int retVal = fcntl(serialPortFD, F_SETFL, flags);
	if (retVal != -1)
		retVal = tcsetattr(serialPortFD, TCSANOW, &options);
	if (baudRateCode == 0)					// Set custom baud rate
		setBaudRate(serialPortFD, baudRate);
	return ((retVal == 0) ? JNI_TRUE : JNI_FALSE);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configEventFlags(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	if (serialPortFD <= 0)
		return JNI_FALSE;

	// Get event flags from Java class
	int baudRate = (*env)->GetIntField(env, obj, baudRateField);
	unsigned int baudRateCode = getBaudRateCode(baudRate);
	int eventsToMonitor = (*env)->GetIntField(env, obj, eventFlagsField);

	// Change read timeouts if we are monitoring data received
	jboolean retVal;
	if ((eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED) > 0)
	{
		struct termios options = { 0 };
		tcgetattr(serialPortFD, &options);
		int flags = fcntl(serialPortFD, F_GETFL);
		if (flags == -1)
			return JNI_FALSE;
		flags &= ~O_NONBLOCK;
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = 10;
		retVal = ((fcntl(serialPortFD, F_SETFL, flags) == -1) || (tcsetattr(serialPortFD, TCSANOW, &options) == -1)) ?
				JNI_FALSE : JNI_TRUE;
		if (baudRateCode == 0)					// Set custom baud rate
			setBaudRate(serialPortFD, baudRate);
	}
	else
		retVal = Java_com_fazecast_jSerialComm_SerialPort_configTimeouts(env, obj, serialPortFD);

	// Apply changes
	return retVal;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_waitForEvent(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	if (serialPortFD <= 0)
		return 0;

	// Initialize the waiting set and the timeouts
	struct timeval timeout = { 1, 0 };
	fd_set waitingSet;
	FD_ZERO(&waitingSet);
	FD_SET(serialPortFD, &waitingSet);

	// Wait for a serial port event
	int retVal;
	do { retVal = select(serialPortFD + 1, &waitingSet, NULL, NULL, &timeout); } while ((retVal < 0) && (errno == EINTR));
	if (retVal <= 0)
		return 0;
	return (FD_ISSET(serialPortFD, &waitingSet)) ? com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE : 0;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_closePortNative(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	// Close port
	if (serialPortFD <= 0)
		return JNI_TRUE;

	// Allow others to open this port
	ioctl(serialPortFD, TIOCNXCL);
	tcdrain(serialPortFD);
	while ((close(serialPortFD) == -1) && (errno != EBADF));
	serialPortFD = -1;
	(*env)->SetLongField(env, obj, serialPortFdField, -1l);
	(*env)->SetBooleanField(env, obj, isOpenedField, JNI_FALSE);

	return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAvailable(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	int numBytesAvailable = -1;
	if (serialPortFD > 0)
		ioctl(serialPortFD, FIONREAD, &numBytesAvailable);

	return numBytesAvailable;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAwaitingWrite(JNIEnv *env, jobject obj, jlong serialPortFD)
{
	int numBytesToWrite = -1;
	if (serialPortFD > 0)
		ioctl(serialPortFD, TIOCOUTQ, &numBytesToWrite);

	return numBytesToWrite;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_readBytes(JNIEnv *env, jobject obj, jlong serialPortFD, jbyteArray buffer, jlong bytesToRead)
{
	// Get port handle and read timeout from Java class
	if (serialPortFD <= 0)
		return -1;
	int timeoutMode = (*env)->GetIntField(env, obj, timeoutModeField);
	int readTimeout = (*env)->GetIntField(env, obj, readTimeoutField);
	int numBytesRead, numBytesReadTotal = 0, bytesRemaining = bytesToRead;
	char* readBuffer = (char*)malloc(bytesToRead);

	// Infinite blocking mode specified, don't return until we have completely finished the read
	if (((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING) > 0) && (readTimeout == 0))
	{
		// While there are more bytes we are supposed to read
		while (bytesRemaining > 0)
		{
			do { numBytesRead = read(serialPortFD, readBuffer+numBytesReadTotal, bytesRemaining); } while ((numBytesRead < 0) && (errno == EINTR));
			if (numBytesRead == -1)
			{
				// Problem reading, allow others to open the port and close it ourselves
				ioctl(serialPortFD, TIOCNXCL);
				tcdrain(serialPortFD);
				while ((close(serialPortFD) == -1) && (errno != EBADF));
				serialPortFD = -1;
				(*env)->SetLongField(env, obj, serialPortFdField, -1l);
				(*env)->SetBooleanField(env, obj, isOpenedField, JNI_FALSE);
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
			do { numBytesRead = read(serialPortFD, readBuffer+numBytesReadTotal, bytesRemaining); } while ((numBytesRead < 0) && (errno == EINTR));
			if (numBytesRead == -1)
			{
				// Problem reading, allow others to open the port and close it ourselves
				ioctl(serialPortFD, TIOCNXCL);
				tcdrain(serialPortFD);
				while ((close(serialPortFD) == -1) && (errno != EBADF));
				serialPortFD = -1;
				(*env)->SetLongField(env, obj, serialPortFdField, -1l);
				(*env)->SetBooleanField(env, obj, isOpenedField, JNI_FALSE);
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
		do { numBytesRead = read(serialPortFD, readBuffer, bytesToRead); } while ((numBytesRead < 0) && (errno == EINTR));
		if (numBytesRead == -1)
		{
			// Problem reading, allow others to open the port and close it ourselves
			ioctl(serialPortFD, TIOCNXCL);
			tcdrain(serialPortFD);
			while ((close(serialPortFD) == -1) && (errno != EBADF));
			serialPortFD = -1;
			(*env)->SetLongField(env, obj, serialPortFdField, -1l);
			(*env)->SetBooleanField(env, obj, isOpenedField, JNI_FALSE);
		}
		else
			numBytesReadTotal = numBytesRead;
	}

	// Return number of bytes read if successful
	(*env)->SetByteArrayRegion(env, buffer, 0, numBytesReadTotal, (jbyte*)readBuffer);
	free(readBuffer);
	return (numBytesRead == -1) ? -1 : numBytesReadTotal;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_writeBytes(JNIEnv *env, jobject obj, jlong serialPortFD, jbyteArray buffer, jlong bytesToWrite)
{
	if (serialPortFD <= 0)
		return -1;
	jbyte *writeBuffer = (*env)->GetByteArrayElements(env, buffer, 0);
	int numBytesWritten, result = 0;

	// Set the DTR line to high if using RS-422
	//ioctl(serialPortFD, TIOCMGET, &result);
	//result |= TIOCM_DTR;
	//ioctl(serialPortFD, TIOCMSET, &result);

	// Write to port
	do { numBytesWritten = write(serialPortFD, writeBuffer, bytesToWrite); } while ((numBytesWritten < 0) && (errno == EINTR));
	if (numBytesWritten == -1)
	{
		// Problem writing, allow others to open the port and close it ourselves
		ioctl(serialPortFD, TIOCNXCL);
		tcdrain(serialPortFD);
		while ((close(serialPortFD) == -1) && (errno != EBADF));
		serialPortFD = -1;
		(*env)->SetLongField(env, obj, serialPortFdField, -1l);
		(*env)->SetBooleanField(env, obj, isOpenedField, JNI_FALSE);
	}

	// Clear the DTR line if using RS-422
//#ifdef TIOCSERGETLSR
	//do
	//{
		//result = ioctl(serialPortFD, TIOCSERGETLSR);
		//if (result != TIOCSER_TEMT)
			//usleep(100);
	//} while (result != TIOCSER_TEMT);
//#endif
	//ioctl(serialPortFD, TIOCMGET, &result);
	//result &= ~TIOCM_DTR;
	//ioctl(serialPortFD, TIOCMSET, &result);
	//do { result = tcflush(serialPortFD, TCIFLUSH); } while ((result < 0) && (errno == EINTR));

	// Return number of bytes written if successful
	(*env)->ReleaseByteArrayElements(env, buffer, writeBuffer, JNI_ABORT);
	return numBytesWritten;
}

#endif
