/*
 * SerialPort_Posix.c
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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
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
jclass jniErrorClass;
jmethodID serialCommConstructor;
jfieldID serialPortFdField;
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
jfieldID disableExclusiveLockField;
jfieldID requestElevatedPermissionsField;
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

// Global list of available serial ports
char portsEnumerated = 0;
char classInitialized = 0;
pthread_mutex_t criticalSection;
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
		serialPorts.ports[i]->enumerated = (serialPorts.ports[i]->handle > 0);

	// Enumerate serial ports on this machine
	searchForComPorts(&serialPorts);

	// Remove all non-enumerated ports from the serial port listing
	for (int i = 0; i < serialPorts.length; ++i)
		if (!serialPorts.ports[i]->enumerated)
		{
			removePort(&serialPorts, serialPorts.ports[i]);
			i--;
		}
	portsEnumerated = 1;
}

#if defined(__linux__) && !defined(__ANDROID__)

// Event listening threads
void* eventReadingThread1(void *serialPortPointer)
{
	// Make this thread immediately and asynchronously cancellable
	int oldValue;
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldValue);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldValue);

	// Loop forever while open
	struct serial_icounter_struct oldSerialLineInterrupts, newSerialLineInterrupts;
	int mask = 1, isSupported = !ioctl(port->handle, TIOCGICOUNT, &oldSerialLineInterrupts);
	while (isSupported && mask && port->eventListenerRunning && port->eventListenerUsesThreads)
	{
		// Determine which modem bit changes to listen for
		mask = 0;
		if (port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_CARRIER_DETECT)
			mask |= TIOCM_CD;
		if (port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_CTS)
			mask |= TIOCM_CTS;
		if (port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DSR)
			mask |= TIOCM_DSR;
		if (port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_RING_INDICATOR)
			mask |= TIOCM_RNG;

		// Listen forever for a change in the modem lines
		isSupported = !ioctl(port->handle, TIOCMIWAIT, mask) && !ioctl(port->handle, TIOCGICOUNT, &newSerialLineInterrupts);

		// Return the detected port events
		if (isSupported)
		{
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldValue);
			pthread_mutex_lock(&port->eventMutex);
			if (newSerialLineInterrupts.dcd != oldSerialLineInterrupts.dcd)
				port->event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_CARRIER_DETECT;
			if (newSerialLineInterrupts.cts != oldSerialLineInterrupts.cts)
				port->event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_CTS;
			if (newSerialLineInterrupts.dsr != oldSerialLineInterrupts.dsr)
				port->event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DSR;
			if (newSerialLineInterrupts.rng != oldSerialLineInterrupts.rng)
				port->event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_RING_INDICATOR;
			memcpy(&oldSerialLineInterrupts, &newSerialLineInterrupts, sizeof(newSerialLineInterrupts));
			if (port->event)
				pthread_cond_signal(&port->eventReceived);
			pthread_mutex_unlock(&port->eventMutex);
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldValue);
		}
	}
	return NULL;
}

void* eventReadingThread2(void *serialPortPointer)
{
	// Make this thread immediately and asynchronously cancellable
	int oldValue;
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldValue);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldValue);
	struct serial_icounter_struct oldSerialLineInterrupts, newSerialLineInterrupts;
	ioctl(port->handle, TIOCGICOUNT, &oldSerialLineInterrupts);

	// Loop forever while open
	while (port->eventListenerRunning && port->eventListenerUsesThreads)
	{
		// Initialize the polling variables
		int pollResult;
		short pollEventsMask = ((port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE) || (port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED)) ? (POLLIN | POLLERR) : POLLERR;
		struct pollfd waitingSet = { port->handle, pollEventsMask, 0 };

		// Wait for a serial port event
		do
		{
			waitingSet.revents = 0;
			pollResult = poll(&waitingSet, 1, 1000);
		}
		while ((pollResult == 0) && port->eventListenerRunning && port->eventListenerUsesThreads);

		// Return the detected port events
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldValue);
		pthread_mutex_lock(&port->eventMutex);
		if (waitingSet.revents & POLLHUP)
			port->event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PORT_DISCONNECTED;
		else if (waitingSet.revents & POLLIN)
			port->event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE;
		if (waitingSet.revents & POLLERR)
			if (!ioctl(port->handle, TIOCGICOUNT, &newSerialLineInterrupts))
			{
				if (oldSerialLineInterrupts.frame != newSerialLineInterrupts.frame)
					port->event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_FRAMING_ERROR;
				if (oldSerialLineInterrupts.brk != newSerialLineInterrupts.brk)
					port->event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_BREAK_INTERRUPT;
				if (oldSerialLineInterrupts.overrun != newSerialLineInterrupts.overrun)
					port->event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_FIRMWARE_OVERRUN_ERROR;
				if (oldSerialLineInterrupts.parity != newSerialLineInterrupts.parity)
					port->event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PARITY_ERROR;
				if (oldSerialLineInterrupts.buf_overrun != newSerialLineInterrupts.buf_overrun)
					port->event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_SOFTWARE_OVERRUN_ERROR;
				memcpy(&oldSerialLineInterrupts, &newSerialLineInterrupts, sizeof(newSerialLineInterrupts));
			}
		if (port->event)
			pthread_cond_signal(&port->eventReceived);
		pthread_mutex_unlock(&port->eventMutex);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldValue);
	}
	return NULL;
}

#endif // #if defined(__linux__)

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
	serialPortFdField = (*env)->GetFieldID(env, serialCommClass, "portHandle", "J");
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
	disableExclusiveLockField = (*env)->GetFieldID(env, serialCommClass, "disableExclusiveLock", "Z");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	requestElevatedPermissionsField = (*env)->GetFieldID(env, serialCommClass, "requestElevatedPermissions", "Z");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	rs485ModeField = (*env)->GetFieldID(env, serialCommClass, "rs485Mode", "Z");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	rs485ActiveHighField = (*env)->GetFieldID(env, serialCommClass, "rs485ActiveHigh", "Z");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	rs485EnableTerminationField = (*env)->GetFieldID(env, serialCommClass, "rs485EnableTermination", "Z");
	if (checkJniError(env, __LINE__ - 1)) return JNI_ERR;
	rs485RxDuringTxField = (*env)->GetFieldID(env, serialCommClass, "rs485RxDuringTx", "Z");
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

	// Disable handling of various POSIX signals
	sigset_t blockMask;
	memset(&blockMask, 0, sizeof(blockMask));
	struct sigaction ignoreAction = { 0 };
	ignoreAction.sa_handler = SIG_IGN;
	ignoreAction.sa_mask = blockMask;
	sigaction(SIGIO, &ignoreAction, NULL);
	sigaction(SIGHUP, &ignoreAction, NULL);
	sigaction(SIGCONT, &ignoreAction, NULL);
	sigaction(SIGUSR1, &ignoreAction, NULL);
	sigaction(SIGUSR2, &ignoreAction, NULL);
	sigaction(SIGTTOU, &ignoreAction, NULL);
	sigaction(SIGTTIN, &ignoreAction, NULL);

	// Initialize the critical section lock
	pthread_mutex_init(&criticalSection, NULL);
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
		if (serialPorts.ports[i]->handle > 0)
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
	pthread_mutex_lock(&criticalSection);

	// Enumerate all ports on the current system
	enumeratePorts();

	// Create a Java-based port listing
	jobjectArray arrayObject = (*env)->NewObjectArray(env, serialPorts.length, serialComm, 0);
	for (int i = 0; !checkJniError(env, __LINE__ - 1) && (i < serialPorts.length); ++i)
	{
		// Create a new SerialComm object containing the enumerated values
		jobject serialCommObject = (*env)->NewObject(env, serialComm, serialCommConstructor);
		if (checkJniError(env, __LINE__ - 1)) break;
		(*env)->SetObjectField(env, serialCommObject, portDescriptionField, (*env)->NewStringUTF(env, serialPorts.ports[i]->portDescription));
		if (checkJniError(env, __LINE__ - 1)) break;
		(*env)->SetObjectField(env, serialCommObject, friendlyNameField, (*env)->NewStringUTF(env, serialPorts.ports[i]->friendlyName));
		if (checkJniError(env, __LINE__ - 1)) break;
		(*env)->SetObjectField(env, serialCommObject, comPortField, (*env)->NewStringUTF(env, serialPorts.ports[i]->portPath));
		if (checkJniError(env, __LINE__ - 1)) break;
		(*env)->SetObjectField(env, serialCommObject, portLocationField, (*env)->NewStringUTF(env, serialPorts.ports[i]->portLocation));
		if (checkJniError(env, __LINE__ - 1)) break;
		(*env)->SetIntField(env, serialCommObject, vendorIdField, serialPorts.ports[i]->vendorID);
		if (checkJniError(env, __LINE__ - 1)) break;
		(*env)->SetIntField(env, serialCommObject, productIdField, serialPorts.ports[i]->productID);
		if (checkJniError(env, __LINE__ - 1)) break;

		// Add new SerialComm object to array
		(*env)->SetObjectArrayElement(env, arrayObject, i, serialCommObject);
	}

	// Exit critical section and return the com port array
	pthread_mutex_unlock(&criticalSection);
	return arrayObject;
}

JNIEXPORT void JNICALL Java_com_fazecast_jSerialComm_SerialPort_retrievePortDetails(JNIEnv *env, jobject obj)
{
	// Retrieve the serial port parameter fields
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	if (checkJniError(env, __LINE__ - 1)) return;
	const char *portName = (*env)->GetStringUTFChars(env, portNameJString, NULL);
	if (checkJniError(env, __LINE__ - 1)) return;

	// Ensure that the serial port exists
	pthread_mutex_lock(&criticalSection);
	if (!portsEnumerated)
		enumeratePorts();
	serialPort *port = fetchPort(&serialPorts, portName);
	char continueRetrieval = (port != NULL);

	// Fill in the Java-side port details
	if (continueRetrieval)
	{
		(*env)->SetObjectField(env, obj, portDescriptionField, (*env)->NewStringUTF(env, port->portDescription));
		if (checkJniError(env, __LINE__ - 1)) continueRetrieval = 0;
	}
	if (continueRetrieval)
	{
		(*env)->SetObjectField(env, obj, friendlyNameField, (*env)->NewStringUTF(env, port->friendlyName));
		if (checkJniError(env, __LINE__ - 1)) continueRetrieval = 0;
	}
	if (continueRetrieval)
	{
		(*env)->SetObjectField(env, obj, portLocationField, (*env)->NewStringUTF(env, port->portLocation));
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
	pthread_mutex_unlock(&criticalSection);
	(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
	checkJniError(env, __LINE__ - 1);
}

JNIEXPORT jlong JNICALL Java_com_fazecast_jSerialComm_SerialPort_openPortNative(JNIEnv *env, jobject obj)
{
	// Retrieve the serial port parameter fields
	jstring portNameJString = (jstring)(*env)->GetObjectField(env, obj, comPortField);
	if (checkJniError(env, __LINE__ - 1)) return 0;
	unsigned char disableExclusiveLock = (*env)->GetBooleanField(env, obj, disableExclusiveLockField);
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
	const char *portName = (*env)->GetStringUTFChars(env, portNameJString, NULL);
	if (checkJniError(env, __LINE__ - 1)) return 0;

	// Ensure that the serial port still exists and is not already open
	pthread_mutex_lock(&criticalSection);
	serialPort *port = fetchPort(&serialPorts, portName);
	if (!port)
	{
		// Create port representation and add to serial port listing
		port = pushBack(&serialPorts, portName, "User-Specified Port", "User-Specified Port", "0-0", -1, -1);
	}
	pthread_mutex_unlock(&criticalSection);
	if (!port || (port->handle > 0))
	{
		(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
		checkJniError(env, __LINE__ - 1);
		lastErrorLineNumber = __LINE__ - 3;
		lastErrorNumber = (!port ? 1 : 2);
		return 0;
	}

	// Fix user permissions so that they can open the port, if allowed
	if (requestElevatedPermissions)
		verifyAndSetUserPortGroup(portName);

	// Try to open the serial port with read/write access
	port->errorLineNumber = lastErrorLineNumber = __LINE__ + 1;
	int portHandle = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
	if (portHandle > 0)
	{
		// Set the newly opened port handle in the serial port structure
		pthread_mutex_lock(&criticalSection);
		port->handle = portHandle;
		pthread_mutex_unlock(&criticalSection);

		// Quickly set the desired RTS/DTR line status immediately upon opening
		if (isDtrEnabled)
			Java_com_fazecast_jSerialComm_SerialPort_setDTR(env, obj, (jlong)(intptr_t)port);
		else
			Java_com_fazecast_jSerialComm_SerialPort_clearDTR(env, obj, (jlong)(intptr_t)port);
		if (isRtsEnabled)
			Java_com_fazecast_jSerialComm_SerialPort_setRTS(env, obj, (jlong)(intptr_t)port);
		else
			Java_com_fazecast_jSerialComm_SerialPort_clearRTS(env, obj, (jlong)(intptr_t)port);

		// Ensure that multiple root users cannot access the device simultaneously
		if (!disableExclusiveLock && flock(port->handle, LOCK_EX | LOCK_NB))
		{
			port->errorLineNumber = lastErrorLineNumber = __LINE__ - 2;
			port->errorNumber = lastErrorNumber = errno;
			while (close(port->handle) && (errno == EINTR))
				errno = 0;
			pthread_mutex_lock(&criticalSection);
			port->handle = -1;
			pthread_mutex_unlock(&criticalSection);
		}
		else if (!disableAutoConfig && !Java_com_fazecast_jSerialComm_SerialPort_configPort(env, obj, (jlong)(intptr_t)port))
		{
			// Close the port if there was a problem setting the parameters
			fcntl(port->handle, F_SETFL, O_NONBLOCK);
			while (close(port->handle) && (errno == EINTR))
				errno = 0;
			pthread_mutex_lock(&criticalSection);
			port->handle = -1;
			pthread_mutex_unlock(&criticalSection);
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
		port->errorNumber = lastErrorNumber = errno;

	// Return a pointer to the serial port data structure
	(*env)->ReleaseStringUTFChars(env, portNameJString, portName);
	checkJniError(env, __LINE__ - 1);
	return (port->handle > 0) ? (jlong)(intptr_t)port : 0;
}

JNIEXPORT jboolean JNICALL Java_com_fazecast_jSerialComm_SerialPort_configPort(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Retrieve port parameters from the Java class
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	baud_rate baudRate = (*env)->GetIntField(env, obj, baudRateField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	int byteSizeInt = (*env)->GetIntField(env, obj, dataBitsField);
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
	unsigned char rs485ModeEnabled = (*env)->GetBooleanField(env, obj, rs485ModeField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	unsigned char isDtrEnabled = (*env)->GetBooleanField(env, obj, isDtrEnabledField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	unsigned char isRtsEnabled = (*env)->GetBooleanField(env, obj, isRtsEnabledField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	char xonStartChar = (*env)->GetByteField(env, obj, xonStartCharField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	char xoffStopChar = (*env)->GetByteField(env, obj, xoffStopCharField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
#if defined(__linux__)
	int sendDeviceQueueSize = (*env)->GetIntField(env, obj, sendDeviceQueueSizeField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	int receiveDeviceQueueSize = (*env)->GetIntField(env, obj, receiveDeviceQueueSizeField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	int rs485DelayBefore = (*env)->GetIntField(env, obj, rs485DelayBeforeField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	int rs485DelayAfter = (*env)->GetIntField(env, obj, rs485DelayAfterField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	unsigned char rs485ActiveHigh = (*env)->GetBooleanField(env, obj, rs485ActiveHighField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	unsigned char rs485EnableTermination = (*env)->GetBooleanField(env, obj, rs485EnableTerminationField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	unsigned char rs485RxDuringTx = (*env)->GetBooleanField(env, obj, rs485RxDuringTxField);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
#endif

	// Clear any serial port flags and set up raw non-canonical port parameters
	struct termios options = { 0 };
	tcgetattr(port->handle, &options);
	options.c_cc[VSTART] = (unsigned char)xonStartChar;
	options.c_cc[VSTOP] = (unsigned char)xoffStopChar;
	options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | INPCK | IGNPAR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
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
		port->errorLineNumber = lastErrorLineNumber = __LINE__ - 2;
		port->errorNumber = lastErrorNumber = errno;
		return JNI_FALSE;
	}

#if defined(__linux__)

	// Attempt to set the transmit buffer size, closing wait time, and latency flags
	struct serial_struct serInfo = { 0 };
	if (!ioctl(port->handle, TIOCGSERIAL, &serInfo))
	{
		serInfo.closing_wait = 250;
		serInfo.xmit_fifo_size = sendDeviceQueueSize;
		serInfo.flags |= ASYNC_LOW_LATENCY;
		ioctl(port->handle, TIOCSSERIAL, &serInfo);
	}

	// Retrieve the driver-reported transmit buffer size
	if (!ioctl(port->handle, TIOCGSERIAL, &serInfo))
		sendDeviceQueueSize = serInfo.xmit_fifo_size;
	receiveDeviceQueueSize = sendDeviceQueueSize;
	(*env)->SetIntField(env, obj, sendDeviceQueueSizeField, sendDeviceQueueSize);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	(*env)->SetIntField(env, obj, receiveDeviceQueueSizeField, receiveDeviceQueueSize);
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;

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
		ioctl(port->handle, TIOCSRS485, &rs485Conf);
	}

#else

	(*env)->SetIntField(env, obj, sendDeviceQueueSizeField, sysconf(_SC_PAGESIZE));
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	(*env)->SetIntField(env, obj, receiveDeviceQueueSizeField, sysconf(_SC_PAGESIZE));
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;

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
	if (checkJniError(env, __LINE__ - 1)) return JNI_FALSE;
	tcgetattr(port->handle, &options);

	// Set up the requested event flags
	port->eventsMask = eventsToMonitor;

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
		port->errorLineNumber = lastErrorLineNumber = __LINE__ - 2;
		port->errorNumber = lastErrorNumber = errno;
		return JNI_FALSE;
	}
	if (tcsetattr(port->handle, TCSANOW, &options) || tcsetattr(port->handle, TCSANOW, &options))
	{
		port->errorLineNumber = lastErrorLineNumber = __LINE__ - 2;
		port->errorNumber = lastErrorNumber = errno;
		return JNI_FALSE;
	}
	if (!getBaudRateCode(baudRate) && setBaudRateCustom(port->handle, baudRate))
	{
		port->errorLineNumber = lastErrorLineNumber = __LINE__ - 2;
		port->errorNumber = lastErrorNumber = errno;
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
	// Initialize local variables
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	jint event = com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_TIMED_OUT;

	// Wait for events differently based on the use of threads
	if (port->eventListenerUsesThreads)
	{
		pthread_mutex_lock(&port->eventMutex);
		if ((port->event & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE) && !Java_com_fazecast_jSerialComm_SerialPort_bytesAvailable(env, obj, serialPortPointer))
			port->event &= ~com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE;
		if (port->event)
		{
			event = port->event;
			port->event = 0;
		}
		else
		{
			struct timeval currentTime;
			gettimeofday(&currentTime, NULL);
			struct timespec timeoutTime = { .tv_sec = 1 + currentTime.tv_sec, .tv_nsec = currentTime.tv_usec * 1000 };
			pthread_cond_timedwait(&port->eventReceived, &port->eventMutex, &timeoutTime);
			if (port->event)
			{
				event = port->event;
				port->event = 0;
			}
		}
		pthread_mutex_unlock(&port->eventMutex);
	}
	else
	{
		// Initialize the local variables
		int pollResult;
		short pollEventsMask = ((port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE) || (port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED)) ? (POLLIN | POLLERR) : POLLERR;
		struct pollfd waitingSet = { port->handle, pollEventsMask, 0 };
#if defined(__linux__)
		struct serial_icounter_struct oldSerialLineInterrupts, newSerialLineInterrupts;
		ioctl(port->handle, TIOCGICOUNT, &oldSerialLineInterrupts);
#endif // #if defined(__linux__)

		// Wait for a serial port event
		do
		{
			waitingSet.revents = 0;
			pollResult = poll(&waitingSet, 1, 500);
		}
		while ((pollResult == 0) && port->eventListenerRunning);

		// Return the detected port events
		if (waitingSet.revents & POLLHUP)
			event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PORT_DISCONNECTED;
		else if (waitingSet.revents & POLLIN)
			event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE;
#if defined(__linux__)
		if (waitingSet.revents & POLLERR)
			if (!ioctl(port->handle, TIOCGICOUNT, &newSerialLineInterrupts))
			{
				if (oldSerialLineInterrupts.frame != newSerialLineInterrupts.frame)
					event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_FRAMING_ERROR;
				if (oldSerialLineInterrupts.brk != newSerialLineInterrupts.brk)
					event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_BREAK_INTERRUPT;
				if (oldSerialLineInterrupts.overrun != newSerialLineInterrupts.overrun)
					event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_FIRMWARE_OVERRUN_ERROR;
				if (oldSerialLineInterrupts.parity != newSerialLineInterrupts.parity)
					event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PARITY_ERROR;
				if (oldSerialLineInterrupts.buf_overrun != newSerialLineInterrupts.buf_overrun)
					event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_SOFTWARE_OVERRUN_ERROR;
			}
#endif // #if defined(__linux__)
	}
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

	// Unblock, unlock, and close the port
	fdatasync(port->handle);
	tcflush(port->handle, TCIOFLUSH);
	flock(port->handle, LOCK_UN | LOCK_NB);
	while (close(port->handle) && (errno == EINTR))
		errno = 0;
	pthread_mutex_lock(&criticalSection);
	port->handle = -1;
	pthread_mutex_unlock(&criticalSection);
	return 0;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAvailable(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Retrieve bytes available to read
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	int numBytesAvailable = -1;
	port->errorLineNumber = __LINE__ + 1;
	ioctl(port->handle, FIONREAD, &numBytesAvailable);
	port->errorNumber = errno;
	return numBytesAvailable;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_bytesAwaitingWrite(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	// Retrieve bytes awaiting write
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	int numBytesToWrite = -1;
	port->errorLineNumber = __LINE__ + 1;
	ioctl(port->handle, TIOCOUTQ, &numBytesToWrite);
	port->errorNumber = errno;
	return numBytesToWrite;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_readBytes(JNIEnv *env, jobject obj, jlong serialPortPointer, jbyteArray buffer, jlong bytesToRead, jlong offset, jint timeoutMode, jint readTimeout)
{
	// Ensure that the allocated read buffer is large enough
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	int numBytesRead = -1, numBytesReadTotal = 0, bytesRemaining = bytesToRead, ioctlResult = 0;
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
			{
				// If all bytes were not successfully read, it is an error
				numBytesRead = -1;
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
			port->errorLineNumber = __LINE__ + 1;
			do { errno = 0; numBytesRead = read(port->handle, port->readBuffer + numBytesReadTotal, bytesRemaining); port->errorNumber = errno; } while ((numBytesRead < 0) && (errno == EINTR));
			if ((numBytesRead == -1) || ((numBytesRead == 0) && (ioctl(port->handle, FIONREAD, &ioctlResult) == -1)))
			{
				// If any bytes were read, return those bytes
				if (!numBytesReadTotal)
					numBytesRead = -1;
				break;
			}

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
		if ((numBytesRead == -1) || ((numBytesRead == 0) && (ioctl(port->handle, FIONREAD, &ioctlResult) == -1)))
			numBytesRead = -1;
		else
			numBytesReadTotal = numBytesRead;
	}

	// Return number of bytes read if successful
	(*env)->SetByteArrayRegion(env, buffer, offset, numBytesReadTotal, (jbyte*)port->readBuffer);
	checkJniError(env, __LINE__ - 1);
	return (numBytesRead == -1) ? -1 : numBytesReadTotal;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_writeBytes(JNIEnv *env, jobject obj, jlong serialPortPointer, jbyteArray buffer, jlong bytesToWrite, jlong offset, jint timeoutMode)
{
	// Retrieve port parameters from the Java class
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	int writeBlockingMode = (timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_WRITE_BLOCKING);
	jbyte *writeBuffer = (*env)->GetByteArrayElements(env, buffer, 0);
	if (checkJniError(env, __LINE__ - 1)) return -1;

	// Write to the port
	int numBytesWritten;
	do {
		errno = 0;
		port->errorLineNumber = __LINE__ + 1;
		numBytesWritten = write(port->handle, writeBuffer + offset, bytesToWrite);
		port->errorNumber = errno;
	} while ((numBytesWritten < 0) && ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)));

	// Wait until all bytes were written in write-blocking mode
	if ((writeBlockingMode > 0) && (numBytesWritten > 0))
		tcdrain(port->handle);

	// Return the number of bytes written if successful
	(*env)->ReleaseByteArrayElements(env, buffer, writeBuffer, JNI_ABORT);
	checkJniError(env, __LINE__ - 1);
	return numBytesWritten;
}

JNIEXPORT void JNICALL Java_com_fazecast_jSerialComm_SerialPort_setEventListeningStatus(JNIEnv *env, jobject obj, jlong serialPortPointer, jboolean eventListenerRunning)
{
	// Create or cancel a separate event listening thread if required
	serialPort *port = (serialPort*)(intptr_t)serialPortPointer;
	port->eventListenerRunning = eventListenerRunning;
#if defined(__linux__) && !defined(__ANDROID__)
	if (eventListenerRunning && ((port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_CARRIER_DETECT) || (port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_CTS) ||
			(port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DSR) || (port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_RING_INDICATOR)))
	{
		port->event = 0;
		if (!port->eventsThread1)
		{
			if (!pthread_create(&port->eventsThread1, NULL, eventReadingThread1, port))
				pthread_detach(port->eventsThread1);
			else
				port->eventsThread1 = 0;
		}
		if (!port->eventsThread2)
		{
			if (!pthread_create(&port->eventsThread2, NULL, eventReadingThread2, port))
				pthread_detach(port->eventsThread2);
			else
				port->eventsThread2 = 0;
		}
		port->eventListenerUsesThreads = 1;
	}
	else if (port->eventListenerUsesThreads)
	{
		port->eventListenerUsesThreads = 0;
		pthread_cancel(port->eventsThread1);
		port->eventsThread1 = 0;
		pthread_cancel(port->eventsThread2);
		port->eventsThread2 = 0;
	}
#endif // #if defined(__linux__)
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
	return serialPortPointer ? ((serialPort*)(intptr_t)serialPortPointer)->errorLineNumber : lastErrorLineNumber;
}

JNIEXPORT jint JNICALL Java_com_fazecast_jSerialComm_SerialPort_getLastErrorCode(JNIEnv *env, jobject obj, jlong serialPortPointer)
{
	return serialPortPointer ? ((serialPort*)(intptr_t)serialPortPointer)->errorNumber : lastErrorNumber;
}
