/*
 * SerialPort_OSX.cpp
 *
 *       Created on:  Feb 25, 2012
 *  Last Updated on:  Feb 27, 2015
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

#ifdef __APPLE__
#ifndef CMSPAR
#define CMSPAR 010000000000
#endif
#include <cstdlib>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/ioss.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include "../com_fazecast_jSerialComm_SerialPort.h"

JNIEXPORT jobjectArray JNICALL Java_com_fazecast_jSerialComm_SerialPort_getCommPorts(JNIEnv *env, jclass serialCommClass)
{
	io_object_t serialPort;
	io_iterator_t serialPortIterator;
	int numValues = 0;
	char portString[1024], comPort[1024];

	// Get relevant SerialComm methods and IDs
	jmethodID serialCommConstructor = env->GetMethodID(serialCommClass, "<init>", "()V");
	jfieldID portStringID = env->GetFieldID(serialCommClass, "portString", "Ljava/lang/String;");
	jfieldID comPortID = env->GetFieldID(serialCommClass, "comPort", "Ljava/lang/String;");

	// Enumerate serial ports on machine
	IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching(kIOSerialBSDServiceValue), &serialPortIterator);
	while (IOIteratorNext(serialPortIterator)) ++numValues;
	IOIteratorReset(serialPortIterator);
	jobjectArray arrayObject = env->NewObjectArray(numValues, serialCommClass, 0);
	for (int i = 0; i < numValues; ++i)
	{
		// Get serial port name and COM value
		serialPort = IOIteratorNext(serialPortIterator);
		CFStringRef portStringRef = (CFStringRef)IORegistryEntryCreateCFProperty(serialPort, CFSTR(kIOTTYDeviceKey), kCFAllocatorDefault, 0);
		CFStringRef comPortRef = (CFStringRef)IORegistryEntryCreateCFProperty(serialPort, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0);
		CFStringGetCString(portStringRef, portString, sizeof(portString), kCFStringEncodingUTF8);
		CFStringGetCString(comPortRef, comPort, sizeof(comPort), kCFStringEncodingUTF8);
		CFRelease(portStringRef);
		CFRelease(comPortRef);

		// Create new SerialComm object containing the enumerated values
		jobject serialCommObject = env->NewObject(serialCommClass, serialCommConstructor);
		env->SetObjectField(serialCommObject, portStringID, env->NewStringUTF(portString));
		env->SetObjectField(serialCommObject, comPortID, env->NewStringUTF(comPort));

		// Add new SerialComm object to array
		env->SetObjectArrayElement(arrayObject, i, serialCommObject);
		IOObjectRelease(serialPort);
	}
	IOObjectRelease(serialPortIterator);

	return arrayObject;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_openPortNative(JNIEnv *env, jobject obj)
{
	int fdSerial;
	jstring portNameJString = (jstring)env->GetObjectField(obj, env->GetFieldID(env->GetObjectClass(obj), "comPort", "Ljava/lang/String;"));
	const char *portName = env->GetStringUTFChars(portNameJString, NULL);

	// Try to open existing serial port with read/write access
	if ((fdSerial = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK)) > 0)
	{
		// Set port handle in Java structure
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), fdSerial);

		// Configure the port parameters and timeouts
		if (Java_com_fazecast_jSerialComm_SerialPort_configPort(env, obj) && Java_com_fazecast_jSerialComm_SerialPort_configFlowControl(env, obj) &&
				Java_com_fazecast_jSerialComm_SerialPort_configEventFlags(env, obj))
			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_TRUE);
		else
		{
			// Close the port if there was a problem setting the parameters
			close(fdSerial);
			fdSerial = -1;
			env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
		}
	}

	env->ReleaseStringUTFChars(portNameJString, portName);
	return (fdSerial == -1) ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configPort(JNIEnv *env, jobject obj)
{
	struct termios options;
	jclass serialCommClass = env->GetObjectClass(obj);
	int portFD = (int)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));

	// Set raw-mode to allow the use of tcsetattr() and ioctl()
	fcntl(portFD, F_SETFL, 0);
	cfmakeraw(&options);

	// Get port parameters from Java class
	speed_t baudRate = env->GetIntField(obj, env->GetFieldID(serialCommClass, "baudRate", "I"));
	int byteSizeInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "dataBits", "I"));
	int stopBitsInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "stopBits", "I"));
	int parityInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "parity", "I"));
	tcflag_t byteSize = (byteSizeInt == 5) ? CS5 : (byteSizeInt == 6) ? CS6 : (byteSizeInt == 7) ? CS7 : CS8;
	tcflag_t stopBits = ((stopBitsInt == com_fazecast_jSerialComm_SerialPort_ONE_STOP_BIT) || (stopBitsInt == com_fazecast_jSerialComm_SerialPort_ONE_POINT_FIVE_STOP_BITS)) ? 0 : CSTOPB;
	tcflag_t parity = (parityInt == com_fazecast_jSerialComm_SerialPort_NO_PARITY) ? 0 : (parityInt == com_fazecast_jSerialComm_SerialPort_ODD_PARITY) ? (PARENB | PARODD) : (parityInt == com_fazecast_jSerialComm_SerialPort_EVEN_PARITY) ? PARENB : (parityInt == com_fazecast_jSerialComm_SerialPort_MARK_PARITY) ? (PARENB | CMSPAR | PARODD) : (PARENB | CMSPAR);

	// Retrieve existing port configuration
	tcgetattr(portFD, &options);

	// Set updated port parameters
	options.c_cflag = (B38400 | byteSize | stopBits | parity | CLOCAL | CREAD);
	if (parityInt == com_fazecast_jSerialComm_SerialPort_SPACE_PARITY)
		options.c_cflag &= ~PARODD;
	options.c_iflag = ((parityInt > 0) ? (INPCK | ISTRIP) : IGNPAR);
	options.c_oflag = 0;
	options.c_lflag = 0;

	// Apply changes
	tcsetattr(portFD, TCSAFLUSH, &options);
	ioctl(portFD, TIOCEXCL);				// Block non-root users from using this port
	return (ioctl(portFD, IOSSIOSPEED, &baudRate) == -1) ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configFlowControl(JNIEnv *env, jobject obj)
{
	struct termios options;
	jclass serialCommClass = env->GetObjectClass(obj);
	int portFD = (int)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));

	// Get port parameters from Java class
	int flowControl = env->GetIntField(obj, env->GetFieldID(serialCommClass, "flowControl", "I"));
	tcflag_t CTSRTSEnabled = (((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_CTS_ENABLED) > 0) ||
			((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_RTS_ENABLED) > 0)) ? CRTSCTS : 0;
	tcflag_t XonXoffInEnabled = ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_XONXOFF_IN_ENABLED) > 0) ? IXOFF : 0;
	tcflag_t XonXoffOutEnabled = ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_XONXOFF_OUT_ENABLED) > 0) ? IXON : 0;

	// Retrieve existing port configuration
	tcgetattr(portFD, &options);

	// Set updated port parameters
	options.c_cflag |= CTSRTSEnabled;
	options.c_iflag |= XonXoffInEnabled | XonXoffOutEnabled;
	options.c_oflag = 0;
	options.c_lflag = 0;

	// Apply changes
	tcsetattr(portFD, TCSAFLUSH, &options);
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configTimeouts(JNIEnv *env, jobject obj)
{
	// Get port timeouts from Java class
	jclass serialCommClass = env->GetObjectClass(obj);
	int serialFD = (int)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));
	int timeoutMode = env->GetIntField(obj, env->GetFieldID(serialCommClass, "timeoutMode", "I"));
	int readTimeout = env->GetIntField(obj, env->GetFieldID(serialCommClass, "readTimeout", "I"));

	// Retrieve existing port configuration
	struct termios options;
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
	else																								// Non-blocking
	{
		flags |= O_NONBLOCK;
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = 0;
	}

	// Apply changes
	fcntl(serialFD, F_SETFL, flags);
	return (tcsetattr(serialFD, TCSAFLUSH, &options) == 0) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_closePortNative(JNIEnv *env, jobject obj)
{
	// Close port
	int portFD = (int)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));
	close(portFD);
	env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
	env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);

	return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAvailable(JNIEnv *env, jobject obj)
{
	int serialPortFD = (int)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));
	int numBytesAvailable = -1;

	if (serialPortFD != -1)
		ioctl(serialPortFD, FIONREAD, &numBytesAvailable);

	return numBytesAvailable;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_readBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToRead)
{
	// Get port handle and read timeout from Java class
	jclass serialCommClass = env->GetObjectClass(obj);
	int timeoutMode = env->GetIntField(obj, env->GetFieldID(serialCommClass, "timeoutMode", "I"));
	int readTimeout = env->GetIntField(obj, env->GetFieldID(serialCommClass, "readTimeout", "I"));
	int serialPortFD = (int)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));
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
				env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
				env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
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
		struct timeval expireTime, currTime;
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
				env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
				env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
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
			env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
		}
		else
			numBytesReadTotal = numBytesRead;
	}

	// Return number of bytes read if successful
	env->SetByteArrayRegion(buffer, 0, numBytesReadTotal, (jbyte*)readBuffer);
	free(readBuffer);
	return (numBytesRead == -1) ? -1 : numBytesReadTotal;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_writeBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToWrite)
{
	int serialPortFD = (int)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));
	if (serialPortFD == -1)
		return -1;
	jbyte *writeBuffer = env->GetByteArrayElements(buffer, 0);
	int numBytesWritten;

	// Write to port
	if ((numBytesWritten = write(serialPortFD, writeBuffer, bytesToWrite)) == -1)
	{
		// Problem writing, close port
		close(serialPortFD);
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
		env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
	}

	// Return number of bytes written if successful
	env->ReleaseByteArrayElements(buffer, writeBuffer, JNI_ABORT);
	return numBytesWritten;
}

#endif
