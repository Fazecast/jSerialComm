/*
 * SerialPort_Posix.c
 *
 *       Created on:  Feb 25, 2012
 *  Last Updated on:  Dec 16, 2021
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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#if defined(__linux__)
#include <linux/serial.h>
#elif defined(__sun__)
#include <sys/filio.h>
#endif
#include "PosixHelperFunctions.h"

// Cached class, method, and field IDs
jclass serialCommClass;
jmethodID serialCommConstructor;
jfieldID serialPortFdField;
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
jfieldID disableExclusiveLockField;
jfieldID rs485ModeField;
jfieldID rs485ActiveHighField;
jfieldID rs485EnableTerminationField;
jfieldID rs485RxDuringTxField;
jfieldID rs485DelayBeforeField;
jfieldID rs485DelayAfterField;
jfieldID xonStartCharField;
jfieldID xoffStopCharField;
jfieldID timeoutModeField;
jfieldID readTimeoutField;
jfieldID writeTimeoutField;
jfieldID eventFlagsField;

// List of available serial ports
serialPortVector serialPorts = { NULL, 0, 0 };

JNIEXPORT jobjectArray JNICALL Java_com_fazecast_jSerialComm_SerialPort_getCommPorts(JNIEnv *env, jclass serialComm)
{
	// Reset the enumerated flag on all non-open serial ports
	for (int i = 0; i < serialPorts.length; ++i)
		serialPorts.ports[i]->enumerated = (serialPorts.ports[i]->handle > 0);

	// Enumerate serial ports on this machine
#if defined(__linux__)

	recursiveSearchForComPorts(&serialPorts, "/sys/devices/");
	driverBasedSearchForComPorts(&serialPorts, "/proc/tty/driver/serial", "/dev/ttyS");
	driverBasedSearchForComPorts(&serialPorts, "/proc/tty/driver/mvebu_serial", "/dev/ttyMV");
	lastDitchSearchForComPorts(&serialPorts);

#elif defined(__sun__) || defined(__APPLE__) || defined(__FreeBSD__)

	searchForComPorts(&serialPorts);

#endif

	// Remove all non-enumerated ports from the serial port listing
	for (int i = 0; i < serialPorts.length; ++i)
		if (!serialPorts.ports[i]->enumerated)
		{
			removePort(&serialPorts, serialPorts.ports[i]);
			i--;
		}

	// Create a Java-based port listing
	jobjectArray arrayObject = (*env)->NewObjectArray(env, serialPorts.length, serialCommClass, 0);
	for (int i = 0; i < serialPorts.length; ++i)
	{
		// Create a new SerialComm object containing the enumerated values
		jobject serialCommObject = (*env)->NewObject(env, serialCommClass, serialCommConstructor);
		(*env)->SetObjectField(env, serialCommObject, portDescriptionField, (*env)->NewStringUTF(env, serialPorts.ports[i]->portDescription));
		(*env)->SetObjectField(env, serialCommObject, friendlyNameField, (*env)->NewStringUTF(env, serialPorts.ports[i]->friendlyName));
		(*env)->SetObjectField(env, serialCommObject, comPortField, (*env)->NewStringUTF(env, serialPorts.ports[i]->portPath));
		(*env)->SetObjectField(env, serialCommObject, portLocationField, (*env)->NewStringUTF(env, serialPorts.ports[i]->portLocation));

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
	serialPortFdField = (*env)->GetFieldID(env, serialCommClass, "portHandle", "J");
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
	disableExclusiveLockField = (*env)->GetFieldID(env, serialCommClass, "disableExclusiveLock", "Z");
	rs485ModeField = (*env)->GetFieldID(env, serialCommClass, "rs485Mode", "Z");
	rs485ActiveHighField = (*env)->GetFieldID(env, serialCommClass, "rs485ActiveHigh", "Z");
	rs485EnableTerminationField = (*env)->GetFieldID(env, serialCommClass, "rs485EnableTermination", "Z");
	rs485RxDuringTxField = (*env)->GetFieldID(env, serialCommClass, "rs485RxDuringTx", "Z");
	rs485DelayBeforeField = (*env)->GetFieldID(env, serialCommClass, "rs485DelayBefore", "I");
	rs485DelayAfterField = (*env)->GetFieldID(env, serialCommClass, "rs485DelayAfter", "I");
	xonStartCharField = (*env)->GetFieldID(env, serialCommClass, "xonStartChar", "B");
	xoffStopCharField = (*env)->GetFieldID(env, serialCommClass, "xoffStopChar", "B");
	timeoutModeField = (*env)->GetFieldID(env, serialCommClass, "timeoutMode", "I");
	readTimeoutField = (*env)->GetFieldID(env, serialCommClass, "readTimeout", "I");
	writeTimeoutField = (*env)->GetFieldID(env, serialCommClass, "writeTimeout", "I");
	eventFlagsField = (*env)->GetFieldID(env, serialCommClass, "eventFlags", "I");

	// Disable handling of various POSIX signals
	sigset_t blockMask;
	sigemptyset(&blockMask);
	struct sigaction ignoreAction = { 0 };
	ignoreAction.sa_handler = SIG_IGN;
	ignoreAction.sa_mask = blockMask;
	sigaction(SIGIO, &ignoreAction, NULL);
	sigaction(SIGINT, &ignoreAction, NULL);
	sigaction(SIGTERM, &ignoreAction, NULL);
	sigaction(SIGCONT, &ignoreAction, NULL);
	sigaction(SIGUSR1, &ignoreAction, NULL);
	sigaction(SIGUSR2, &ignoreAction, NULL);
	sigaction(SIGTTOU, &ignoreAction, NULL);
	sigaction(SIGTTIN, &ignoreAction, NULL);
}

JNIEXPORT void JNICALL Java_com_fazecast_jSerialComm_SerialPort_uninitializeLibrary(JNIEnv *env, jclass serialComm)
{
	// Delete the cached global reference
	(*env)->DeleteGlobalRef(env, serialCommClass);
}

JNIEXPORT jlong JNICALL Java_com_fazecast_jSerialComm_SerialPort_openPortNative(JNIEnv *env, jobject obj)
{
	// Retrieve the serial port parameter fields
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	const char *portName = (*env)->GetStringUTFChars(env, portNameJString, NULL);
	unsigned char disableExclusiveLock = (*env)->GetBooleanField(env, obj, disableExclusiveLockField);
	unsigned char disableAutoConfig = (*env)->GetBooleanField(env, obj, disableConfigField);
	unsigned char autoFlushIOBuffers = (*env)->GetBooleanField(env, obj, autoFlushIOBuffersField);

	// Ensure that the serial port still exists and is not already open
	serialPort *port = fetchPort(&serialPorts, portName);
	if (!port)
	{
		// Create port representation and add to serial port listing
		port = pushBack(&serialPorts, portName, "User-Specified Port", "User-Specified Port", "0-0");
	}
	if (!port || (port->handle > 0))
	{
		(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
		return 0;
	}

	// Try to open the serial port with read/write access
	port->errorLineNumber = __LINE__ + 1;
	if ((port->handle = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC)) > 0)
	{
		// Ensure that multiple root users cannot access the device simultaneously
		if (!disableExclusiveLock && flock(port->handle, LOCK_EX | LOCK_NB))
		{
			port->errorLineNumber = __LINE__ - 2;
			port->errorNumber = errno;
			while (close(port->handle) && (errno == EINTR))
				errno = 0;
			port->handle = -1;
		}
		else if (!disableAutoConfig && !Java_com_fazecast_jSerialComm_SerialPort_configPort(env, obj, (jlong)(intptr_t)port))
		{
			// Close the port if there was a problem setting the parameters
			fcntl(port->handle, F_SETFL, O_NONBLOCK);
			while (close(port->handle) && (errno == EINTR))
				errno = 0;
			port->handle = -1;
		}
		else if (autoFlushIOBuffers)
		{
			// Sleep to workaround kernel bug about flushing immediately after opening
			const struct timespec sleep_time = { 0, 10000000 };
			nanosleep(&sleep_time, NULL);
			Java_com_fazecast_jSerialComm_SerialPort_flushRxTxBuffers(env, obj, (jlong)(intptr_t)port);
		}
	}
	else
		port->errorNumber = errno;

	// Return a pointer to the serial port data structure
	(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
	return (port->handle > 0) ? (jlong)(intptr_t)port : 0;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configPort(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Retrieve port parameters from the Java class
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	baud_rate baudRate = (*env)->GetIntField(env, obj, baudRateField);
	int byteSizeInt = (*env)->GetIntField(env, obj, dataBitsField);
	int stopBitsInt = (*env)->GetIntField(env, obj, stopBitsField);
	int parityInt = (*env)->GetIntField(env, obj, parityField);
	int flowControl = (*env)->GetIntField(env, obj, flowControlField);
	int sendDeviceQueueSize = (*env)->GetIntField(env, obj, sendDeviceQueueSizeField);
	int receiveDeviceQueueSize = (*env)->GetIntField(env, obj, receiveDeviceQueueSizeField);
	int rs485DelayBefore = (*env)->GetIntField(env, obj, rs485DelayBeforeField);
	int rs485DelayAfter = (*env)->GetIntField(env, obj, rs485DelayAfterField);
	int timeoutMode = (*env)->GetIntField(env, obj, timeoutModeField);
	int readTimeout = (*env)->GetIntField(env, obj, readTimeoutField);
	int writeTimeout = (*env)->GetIntField(env, obj, writeTimeoutField);
	int eventsToMonitor = (*env)->GetIntField(env, obj, eventFlagsField);
	unsigned char rs485ModeEnabled = (*env)->GetBooleanField(env, obj, rs485ModeField);
	unsigned char rs485ActiveHigh = (*env)->GetBooleanField(env, obj, rs485ActiveHighField);
	unsigned char rs485EnableTermination = (*env)->GetBooleanField(env, obj, rs485EnableTerminationField);
	unsigned char rs485RxDuringTx = (*env)->GetBooleanField(env, obj, rs485RxDuringTxField);
	unsigned char isDtrEnabled = (*env)->GetBooleanField(env, obj, isDtrEnabledField);
	unsigned char isRtsEnabled = (*env)->GetBooleanField(env, obj, isRtsEnabledField);
	char xonStartChar = (*env)->GetByteField(env, obj, xonStartCharField);
	char xoffStopChar = (*env)->GetByteField(env, obj, xoffStopCharField);

	// Clear any serial port flags and set up raw non-canonical port parameters
	struct termios options = { 0 };
	tcgetattr(port->handle, &options);
	options.c_cc[VSTART] = (unsigned char)xonStartChar;
	options.c_cc[VSTOP] = (unsigned char)xoffStopChar;
	options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | INPCK | IGNPAR | IGNCR | ICRNL | IXON | IXOFF);
	options.c_oflag &= ~OPOST;
	options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	options.c_cflag &= ~(CSIZE | PARENB | CMSPAR | PARODD | CSTOPB | CRTSCTS);

	// Update the user-specified port parameters
	tcflag_t byteSize = (byteSizeInt == 5) ? CS5 : (byteSizeInt == 6) ? CS6 : (byteSizeInt == 7) ? CS7 : CS8;
	tcflag_t parity = (parityInt == com_fazecast_jSerialComm_SerialPort_NO_PARITY) ? 0 : (parityInt == com_fazecast_jSerialComm_SerialPort_ODD_PARITY) ? (PARENB | PARODD) : (parityInt == com_fazecast_jSerialComm_SerialPort_EVEN_PARITY) ? PARENB : (parityInt == com_fazecast_jSerialComm_SerialPort_MARK_PARITY) ? (PARENB | CMSPAR | PARODD) : (PARENB | CMSPAR);
	options.c_cflag |= (byteSize | parity | CLOCAL | CREAD);
	if (!isDtrEnabled || !isRtsEnabled)
		options.c_cflag &= ~HUPCL;
	if (!rs485ModeEnabled)
		options.c_iflag |= BRKINT;
	if (stopBitsInt == com_fazecast_jSerialComm_SerialPort_TWO_STOP_BITS)
		options.c_cflag |= CSTOPB;
	if (((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_CTS_ENABLED) > 0) || ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_RTS_ENABLED) > 0))
		options.c_cflag |= CRTSCTS;
	if (byteSizeInt < 8)
		options.c_iflag |= ISTRIP;
	if (parityInt != 0)
		options.c_iflag |= (INPCK | IGNPAR);
	if ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_XONXOFF_IN_ENABLED) > 0)
		options.c_iflag |= IXOFF;
	if ((flowControl & com_fazecast_jSerialComm_SerialPort_FLOW_CONTROL_XONXOFF_OUT_ENABLED) > 0)
		options.c_iflag |= IXON;

	// Set the baud rate and apply all changes
	baud_rate baudRateCode = getBaudRateCode(baudRate);
	if (!baudRateCode)
		baudRateCode = B38400;
	cfsetispeed(&options, baudRateCode);
	cfsetospeed(&options, baudRateCode);
	if (tcsetattr(port->handle, TCSANOW, &options) || tcsetattr(port->handle, TCSANOW, &options))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = errno;
		return JNI_FALSE;
	}

#if defined(__linux__)

	// Attempt to set the transmit buffer size
	struct serial_struct serInfo = { 0 };
	if (!ioctl(port->handle, TIOCGSERIAL, &serInfo))
	{
		serInfo.xmit_fifo_size = sendDeviceQueueSize;
		serInfo.flags |= ASYNC_LOW_LATENCY;
		ioctl(port->handle, TIOCSSERIAL, &serInfo);
	}

	// Attempt to set the requested RS-485 mode
	struct serial_rs485 rs485Conf = { 0 };
	if (!ioctl(port->handle, TIOCGRS485, &rs485Conf))
	{
		if (rs485ModeEnabled)
			rs485Conf.flags |= SER_RS485_ENABLED;
		else
			rs485Conf.flags &= ~SER_RS485_ENABLED;
		if (rs485ActiveHigh)
		{
			rs485Conf.flags |= SER_RS485_RTS_ON_SEND;
			rs485Conf.flags &= ~(SER_RS485_RTS_AFTER_SEND);
		}
		else
		{
			rs485Conf.flags &= ~(SER_RS485_RTS_ON_SEND);
			rs485Conf.flags |= SER_RS485_RTS_AFTER_SEND;
		}
		if (rs485RxDuringTx)
			rs485Conf.flags |= SER_RS485_RX_DURING_TX;
		else
			rs485Conf.flags &= ~(SER_RS485_RX_DURING_TX);
		if (rs485EnableTermination)
			rs485Conf.flags |= SER_RS485_TERMINATE_BUS;
		else
			rs485Conf.flags &= ~(SER_RS485_TERMINATE_BUS);
		rs485Conf.delay_rts_before_send = rs485DelayBefore / 1000;
		rs485Conf.delay_rts_after_send = rs485DelayAfter / 1000;
		if (ioctl(port->handle, TIOCSRS485, &rs485Conf))
		{
			port->errorLineNumber = __LINE__ - 2;
			port->errorNumber = errno;
			return JNI_FALSE;
		}
	}

#endif

	// Configure the serial port read and write timeouts
	return Java_com_fazecast_jSerialComm_SerialPort_configTimeouts(env, obj, serialPortPointer, timeoutMode, readTimeout, writeTimeout, eventsToMonitor);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configTimeouts(JNIEnv *env, jobject obj, jlong serialPortPointer, jint timeoutMode, jint readTimeout, jint writeTimeout, jint eventsToMonitor)
{
	// Retrieve the existing port configuration
	int flags = 0;
	struct termios options = { 0 };
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	baud_rate baudRate = (*env)->GetIntField(env, obj, baudRateField);
	tcgetattr(port->handle, &options);

	// Set up the requested event flags
	port->eventsMask = 0;
	if ((eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE) || (eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED))
		port->eventsMask |= POLLIN;

	// Set updated port timeouts
	if ((eventsToMonitor & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED) > 0)
	{
		// Force specific read timeouts if we are monitoring data received
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = 10;
	}
	else if (((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_SEMI_BLOCKING) > 0) && (readTimeout > 0))	// Read Semi-blocking with timeout
	{
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = readTimeout / 100;
	}
	else if ((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_SEMI_BLOCKING) > 0)						// Read Semi-blocking without timeout
	{
		options.c_cc[VMIN] = 1;
		options.c_cc[VTIME] = 0;
	}
	else if (((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING) > 0) && (readTimeout > 0))		// Read Blocking with timeout
	{
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = readTimeout / 100;
	}
	else if ((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING) > 0)								// Read Blocking without timeout
	{
		options.c_cc[VMIN] = 1;
		options.c_cc[VTIME] = 0;
	}
	else if ((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_SCANNER) > 0)									// Scanner Mode
	{
		options.c_cc[VMIN] = 1;
		options.c_cc[VTIME] = 1;
	}
	else																												// Non-blocking
	{
		flags = O_NONBLOCK;
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = 0;
	}

	// Apply changes
	if (fcntl(port->handle, F_SETFL, flags))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = errno;
		return JNI_FALSE;
	}
	if (tcsetattr(port->handle, TCSANOW, &options) || tcsetattr(port->handle, TCSANOW, &options))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = errno;
		return JNI_FALSE;
	}
	if (!getBaudRateCode(baudRate) && setBaudRateCustom(port->handle, baudRate))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = errno;
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_flushRxTxBuffers(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	if (tcflush(port->handle, TCIOFLUSH))
	{
		port->errorLineNumber = __LINE__ - 2;
		port->errorNumber = errno;
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_waitForEvent(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Initialize the local variables
	int pollResult;
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	struct pollfd waitingSet = { port->handle, port->eventsMask, 0 };
	jint event = com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_TIMED_OUT;

	// TODO: LISTEN FOR ERROR EVENTS IN CASE SERIAL PORT GETS UNPLUGGED? TEST IF WORKS?
	/*
	ioctl: TIOCMIWAIT
	ioctl: TIOCGICOUNT
	*/

	// Wait for a serial port event
	do
	{
		waitingSet.revents = 0;
		pollResult = poll(&waitingSet, 1, 500);
	}
	while ((pollResult == 0) && port->eventListenerRunning);

	// Return the detected port events
	if (waitingSet.revents & POLLIN)
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE;
	return event;
}

JNIEXPORT jlong JNICALL Java_com_fazecast_jSerialComm_SerialPort_closePortNative(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Force the port to enter non-blocking mode to ensure that any current reads return
	struct termios options = { 0 };
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	tcgetattr(port->handle, &options);
	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 0;
	fcntl(port->handle, F_SETFL, O_NONBLOCK);
	tcsetattr(port->handle, TCSANOW, &options);
	tcsetattr(port->handle, TCSANOW, &options);

	// Unblock, unlock, and close the port
	fsync(port->handle);
	tcdrain(port->handle);
	tcflush(port->handle, TCIOFLUSH);
	flock(port->handle, LOCK_UN | LOCK_NB);
	while (close(port->handle) && (errno == EINTR))
		errno = 0;
	port->handle = -1;
	return -1;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAvailable(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Retrieve bytes available to read
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	int numBytesAvailable = -1;
	port->errorLineNumber = __LINE__ + 1;
	ioctl(((serialPort*)(intptr_t)serialPortPointer)->handle, FIONREAD, &numBytesAvailable);
	port->errorNumber = errno;
	return numBytesAvailable;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAwaitingWrite(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Retrieve bytes awaiting write
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	int numBytesToWrite = -1;
	port->errorLineNumber = __LINE__ + 1;
	ioctl(((serialPort*)(intptr_t)serialPortPointer)->handle, TIOCOUTQ, &numBytesToWrite);
	port->errorNumber = errno;
	return numBytesToWrite;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_readBytes(JNIEnv *env, jobject obj, jlong serialPortPointer, jbyteArray buffer, jlong bytesToRead, jlong offset, jint timeoutMode, jint readTimeout)
{
	// Ensure that the allocated read buffer is large enough
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	int numBytesRead, numBytesReadTotal = 0, bytesRemaining = bytesToRead, ioctlResult = 0;
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

	// Infinite blocking mode specified, don't return until we have completely finished the read
	if (((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING) > 0) && (readTimeout == 0))
	{
		// While there are more bytes we are supposed to read
		while (bytesRemaining > 0)
		{
			// Attempt to read some number of bytes from the serial port
			port->errorLineNumber = __LINE__ + 1;
			do { errno = 0; numBytesRead = read(port->handle, port->readBuffer + numBytesReadTotal, bytesRemaining); port->errorNumber = errno; } while ((numBytesRead < 0) && (errno == EINTR));
			if ((numBytesRead == -1) || ((numBytesRead == 0) && (ioctl(port->handle, FIONREAD, &ioctlResult) == -1)))
				break;

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
			port->errorLineNumber = __LINE__ + 1;
			do { errno = 0; numBytesRead = read(port->handle, port->readBuffer + numBytesReadTotal, bytesRemaining); port->errorNumber = errno; } while ((numBytesRead < 0) && (errno == EINTR));
			if ((numBytesRead == -1) || ((numBytesRead == 0) && (ioctl(port->handle, FIONREAD, &ioctlResult) == -1)))
				break;

			// Fix index variables
			numBytesReadTotal += numBytesRead;
			bytesRemaining -= numBytesRead;

			// Get current system time
			gettimeofday(&currTime, NULL);
		} while ((bytesRemaining > 0) && ((expireTime.tv_sec > currTime.tv_sec) || ((expireTime.tv_sec == currTime.tv_sec) && (expireTime.tv_usec > currTime.tv_usec))));
	}
	else		// Semi- or non-blocking specified
	{
		// Read from the port
		port->errorLineNumber = __LINE__ + 1;
		do { errno = 0; numBytesRead = read(port->handle, port->readBuffer, bytesToRead); port->errorNumber = errno; } while ((numBytesRead < 0) && (errno == EINTR));
		if (numBytesRead > 0)
			numBytesReadTotal = numBytesRead;
	}

	// Return number of bytes read if successful
	(*env)->SetByteArrayRegion(env, buffer, offset, numBytesReadTotal, (jbyte*)port->readBuffer);
	return (numBytesRead == -1) ? -1 : numBytesReadTotal;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_writeBytes(JNIEnv *env, jobject obj, jlong serialPortPointer, jbyteArray buffer, jlong bytesToWrite, jlong offset, jint timeoutMode)
{
	// Retrieve port parameters from the Java class
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	jbyte *writeBuffer = (*env)->GetByteArrayElements(env, buffer, 0);
	int numBytesWritten, result = 0, ioctlResult = 0;

	// Write to the port
	do {
		errno = 0;
		port->errorLineNumber = __LINE__ + 1;
		numBytesWritten = write(port->handle, writeBuffer + offset, bytesToWrite);
		port->errorNumber = errno;
	} while ((numBytesWritten < 0) && (errno == EINTR));

	// Wait until all bytes were written in write-blocking mode
	if (((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_WRITE_BLOCKING) > 0) && (numBytesWritten > 0))
		tcdrain(port->handle);

	// Return the number of bytes written if successful
	(*env)->ReleaseByteArrayElements(env, buffer, writeBuffer, JNI_ABORT);
	return numBytesWritten;
}

JNIEXPORT void JNICALL Java_com_fazecast_jSerialComm_SerialPort_setEventListeningStatus(JNIEnv *env, jobject obj, jlong serialPortPointer, jboolean eventListenerRunning)
{
	((serialPort*)(intptr_t)serialPortPointer)->eventListenerRunning = eventListenerRunning;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_setBreak(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	port->errorLineNumber = __LINE__ + 1;
	if (ioctl(port->handle, TIOCSBRK))
	{
		port->errorNumber = errno;
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_clearBreak(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	port->errorLineNumber = __LINE__ + 1;
	if (ioctl(port->handle, TIOCCBRK))
	{
		port->errorNumber = errno;
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_setRTS(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	const int modemBits = TIOCM_RTS;
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	port->errorLineNumber = __LINE__ + 1;
	if (ioctl(port->handle, TIOCMBIS, &modemBits))
	{
		port->errorNumber = errno;
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_clearRTS(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	const int modemBits = TIOCM_RTS;
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	port->errorLineNumber = __LINE__ + 1;
	if (ioctl(port->handle, TIOCMBIC, &modemBits))
	{
		port->errorNumber = errno;
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_presetRTS(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	const char *portName = (*env)->GetStringUTFChars(env, portNameJString, NULL);

	// Send a system command to preset the RTS mode of the serial port
	char commandString[128];
#if defined(__linux__)
	sprintf(commandString, "stty -F %s hupcl >>/dev/null 2>&1", portName);
#else
	sprintf(commandString, "stty -f %s hupcl >>/dev/null 2>&1", portName);
#endif
	int result = system(commandString);

	(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
	return (result == 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_preclearRTS(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	const char *portName = (*env)->GetStringUTFChars(env, portNameJString, NULL);

	// Send a system command to preset the RTS mode of the serial port
	char commandString[128];
#if defined(__linux__)
	sprintf(commandString, "stty -F %s -hupcl >>/dev/null 2>&1", portName);
#else
	sprintf(commandString, "stty -f %s -hupcl >>/dev/null 2>&1", portName);
#endif
	int result = system(commandString);

	(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
	return (result == 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_setDTR(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	const int modemBits = TIOCM_DTR;
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	port->errorLineNumber = __LINE__ + 1;
	if (ioctl(port->handle, TIOCMBIS, &modemBits))
	{
		port->errorNumber = errno;
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_clearDTR(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	const int modemBits = TIOCM_DTR;
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	port->errorLineNumber = __LINE__ + 1;
	if (ioctl(port->handle, TIOCMBIC, &modemBits))
	{
		port->errorNumber = errno;
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_presetDTR(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	const char *portName = (*env)->GetStringUTFChars(env, portNameJString, NULL);

	// Send a system command to preset the DTR mode of the serial port
	char commandString[128];
#if defined(__linux__)
	sprintf(commandString, "stty -F %s hupcl >>/dev/null 2>&1", portName);
#else
	sprintf(commandString, "stty -f %s hupcl >>/dev/null 2>&1", portName);
#endif
	int result = system(commandString);

	(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
	return (result == 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_preclearDTR(JNIEnv *env, jobject obj)
{
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	const char *portName = (*env)->GetStringUTFChars(env, portNameJString, NULL);

	// Send a system command to preclear the DTR mode of the serial port
	char commandString[128];
#if defined(__linux__)
	sprintf(commandString, "stty -F %s -hupcl >>/dev/null 2>&1", portName);
#else
	sprintf(commandString, "stty -f %s -hupcl >>/dev/null 2>&1", portName);
#endif
	int result = system(commandString);

	(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
	return (result == 0);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getCTS(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	int modemBits = 0;
	return (ioctl(((serialPort*)(intptr_t)serialPortPointer)->handle, TIOCMGET, &modemBits) == 0) && (modemBits & TIOCM_CTS);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getDSR(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	int modemBits = 0;
	return (ioctl(((serialPort*)(intptr_t)serialPortPointer)->handle, TIOCMGET, &modemBits) == 0) && (modemBits & TIOCM_DSR);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getDCD(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	int modemBits = 0;
	return (ioctl(((serialPort*)(intptr_t)serialPortPointer)->handle, TIOCMGET, &modemBits) == 0) && (modemBits & TIOCM_CAR);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getDTR(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	int modemBits = 0;
	return (ioctl(((serialPort*)(intptr_t)serialPortPointer)->handle, TIOCMGET, &modemBits) == 0) && (modemBits & TIOCM_DTR);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getRTS(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	int modemBits = 0;
	return (ioctl(((serialPort*)(intptr_t)serialPortPointer)->handle, TIOCMGET, &modemBits) == 0) && (modemBits & TIOCM_RTS);
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_getRI(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	int modemBits = 0;
	return (ioctl(((serialPort*)(intptr_t)serialPortPointer)->handle, TIOCMGET, &modemBits) == 0) && (modemBits & TIOCM_RI);
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_getLastErrorLocation(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	return ((serialPort*)(intptr_t)serialPortPointer)->errorLineNumber;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_getLastErrorCode(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	return ((serialPort*)(intptr_t)serialPortPointer)->errorNumber;
}
