#define WINVER _WIN32_WINNT_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA
#define NTDDI_VERSION NTDDI_VISTA
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
#include "../../main/c/Windows/ftdi/ftd2xx.h"
#include "WindowsHelperFunctions.h"

static const int eventsToMonitor = com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED | com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PORT_DISCONNECTED;
static const int timeoutMode = com_fazecast_jSerialComm_SerialPort_TIMEOUT_NONBLOCKING;
static const DWORD readTimeout = 0, writeTimeout = 0;

BOOL configPort(void *portHandle)
{
	// Retrieve existing port configuration
	DCB dcbSerialParams;
	memset(&dcbSerialParams, 0, sizeof(DCB));
	dcbSerialParams.DCBlength = sizeof(DCB);
	DWORD receiveDeviceQueueSize = 4096, sendDeviceQueueSize = 4096;
	if (!SetupComm(portHandle, receiveDeviceQueueSize, sendDeviceQueueSize) || !GetCommState(portHandle, &dcbSerialParams))
	{
		printf("Error Line = %d, Code = %d\n", __LINE__ - 2, GetLastError());
		return FALSE;
	}

	// Set updated port parameters
	dcbSerialParams.BaudRate = 9600;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity = NOPARITY;
	dcbSerialParams.fParity = FALSE;
	dcbSerialParams.fBinary = TRUE;
	dcbSerialParams.fAbortOnError = FALSE;
	dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE;
	dcbSerialParams.fOutxCtsFlow = FALSE;
	dcbSerialParams.fOutxDsrFlow = FALSE;
	dcbSerialParams.fDtrControl = DTR_CONTROL_DISABLE;
	dcbSerialParams.fDsrSensitivity = FALSE;
	dcbSerialParams.fOutX = FALSE;
	dcbSerialParams.fInX = FALSE;
	dcbSerialParams.fTXContinueOnXoff = TRUE;
	dcbSerialParams.fErrorChar = FALSE;
	dcbSerialParams.fNull = FALSE;
	dcbSerialParams.XonLim = 2048;
	dcbSerialParams.XoffLim = 512;
	dcbSerialParams.XonChar = 17;
	dcbSerialParams.XoffChar = 19;

	// Apply changes
	if (!SetCommState(portHandle, &dcbSerialParams))
	{
		printf("Error Line = %d, Code = %d\n", __LINE__ - 2, GetLastError());
		return FALSE;
	}

	// Get event flags from the Java class
	int eventFlags = EV_ERR;
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
	if (!SetCommTimeouts(portHandle, &timeouts) || !SetCommMask(portHandle, eventFlags))
	{
		printf("Error Line = %d, Code = %d\n", __LINE__ - 2, GetLastError());
		return FALSE;
	}
	return TRUE;
}

void* openPortNative(const char *portName)
{
	// Try to open the serial port with read/write access
	void *portHandle = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED, NULL);
	if (portHandle != INVALID_HANDLE_VALUE)
	{
		// Configure the port parameters and timeouts
		if (!configPort(portHandle))
		{
			// Close the port if there was a problem setting the parameters
			PurgeComm(portHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
			if (CancelIoEx)
				CancelIoEx(portHandle, NULL);
			SetCommMask(portHandle, 0);
			CloseHandle(portHandle);
			portHandle = INVALID_HANDLE_VALUE;
		}
	}
	return portHandle;
}

void closePortNative(void *portHandle)
{
	// Force the port to enter non-blocking mode to ensure that any current reads return
	COMMTIMEOUTS timeouts;
	memset(&timeouts, 0, sizeof(COMMTIMEOUTS));
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.ReadIntervalTimeout = MAXDWORD;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 0;
	timeouts.WriteTotalTimeoutConstant = 0;
	SetCommTimeouts(portHandle, &timeouts);

	// Purge any outstanding port operations
	PurgeComm(portHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
	if (CancelIoEx)
		CancelIoEx(portHandle, NULL);
	SetCommMask(portHandle, 0);

	// Close the port
	if (!CloseHandle(portHandle))
		printf("Error Line = %d, Code = %d\n", __LINE__ - 1, GetLastError());
}

int waitForEvent(void *portHandle)
{
	// Create an asynchronous event structure
	OVERLAPPED overlappedStruct;
	memset(&overlappedStruct, 0, sizeof(OVERLAPPED));
	overlappedStruct.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	int event = com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_TIMED_OUT;
	if (!overlappedStruct.hEvent)
	{
		printf("Error Line = %d, Code = %d\n", __LINE__ - 3, GetLastError());
		return event;
	}

	// Wait for a serial port event
	DWORD eventMask = 0, errorMask = 0, waitValue, numBytesTransferred;
	if (!WaitCommEvent(portHandle, &eventMask, &overlappedStruct))
	{
		if ((GetLastError() == ERROR_IO_PENDING) || (GetLastError() == ERROR_INVALID_PARAMETER))
		{
			do { waitValue = WaitForSingleObject(overlappedStruct.hEvent, 500); }
			while (waitValue == WAIT_TIMEOUT);
			if ((waitValue != WAIT_OBJECT_0) || !GetOverlappedResult(portHandle, &overlappedStruct, &numBytesTransferred, FALSE))
			{
				event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PORT_DISCONNECTED;
				printf("Error Line = %d, Code = %d, Wait Value = %d\n", __LINE__ - 3, GetLastError(), waitValue);
				CloseHandle(overlappedStruct.hEvent);
				return event;
			}
		}
		else		// Problem occurred
		{
			event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PORT_DISCONNECTED;
			printf("Error Line = %d, Code = %d\n", __LINE__ - 17, GetLastError());
			CloseHandle(overlappedStruct.hEvent);
			return event;
		}
	}

	// Retrieve and clear any serial port errors
	COMSTAT commInfo;
	if (ClearCommError(portHandle, &errorMask, &commInfo))
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

int main(int argc, char *argv[])
{
   // Check for correct input parameters
   if (argc != 2)
   {
	  printf("USAGE: ./testEventsWindows [PORT_FILE_NAME]\n");
	  return -1;
   }

   // Open the serial port
   void *portHandle = INVALID_HANDLE_VALUE;
   if ((portHandle = openPortNative(argv[1])) == INVALID_HANDLE_VALUE)
   {
      printf("ERROR: Could not open port: %s\n", argv[1]);
      return -2;
   }
   printf("Port opened\n");

   // Wait forever for incoming events
   int events = 0;
   while ((events & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PORT_DISCONNECTED) == 0)
   {
	   events = waitForEvent(portHandle);
	   printf("Received Events: %d\n", events);
	   if (events & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PORT_DISCONNECTED)
		   printf("   Including LISTENING_EVENT_PORT_DISCONNECTED\n");
	   if (events & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE)
		   printf("   Including LISTENING_EVENT_DATA_AVAILABLE\n");
	   if (events & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_WRITTEN)
		   printf("   Including LISTENING_EVENT_DATA_WRITTEN\n");
   }

   // Close the serial port
   closePortNative(portHandle);
   return 0;
}
