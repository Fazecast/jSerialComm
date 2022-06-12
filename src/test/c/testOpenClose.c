#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/time.h>
#include <unistd.h>
#if defined(__linux__)
#include <linux/serial.h>
#define termios asmtermios
#define termio asmtermio
#define winsize asmwinsize
#include <asm/ioctls.h>
#include <asm/termios.h>
#undef termio
#undef termios
#undef winsize
#ifndef SER_RS485_TERMINATE_BUS
#define SER_RS485_TERMINATE_BUS (1 << 5)
#endif
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/ioss.h>
#endif
#include <sys/ioctl.h>
#include <termios.h>
#include "PosixHelperFunctions.h"


// Global static variables
static volatile long portHandle = -1, readBufferLength = 0;
static char *comPort = NULL, *readBuffer = NULL;


// JNI functionality
bool configTimeouts(long serialPortFD, int timeoutMode, int readTimeout, int writeTimeout, int eventsToMonitor)
{
   // Get port timeouts from Java class
   int flags = 0;
   struct termios options = { 0 };
   baud_rate baudRate = 9600;
   tcgetattr(serialPortFD, &options);

   // Set updated port timeouts
   if (((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_SEMI_BLOCKING) > 0) && (readTimeout > 0))	// Read Semi-blocking with timeout
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
   if (fcntl(serialPortFD, F_SETFL, flags))
      return false;
   if (tcsetattr(serialPortFD, TCSANOW, &options) || tcsetattr(serialPortFD, TCSANOW, &options))
      return false;
   if (!getBaudRateCode(baudRate) && setBaudRateCustom(serialPortFD, baudRate))
      return false;
   return true;
}

bool configPort(long serialPortFD)
{
   // Get port parameters from Java class
   baud_rate baudRate = 9600;
   int byteSizeInt = 8;
   int stopBitsInt = 1;
   int parityInt = 0;
   int flowControl = 0;
   int sendDeviceQueueSize = 4096;
   int receiveDeviceQueueSize = 4096;
   int rs485DelayBefore = 0;
   int rs485DelayAfter = 0;
   int timeoutMode = 0;
   int readTimeout = 0;
   int writeTimeout = 0;
   int eventsToMonitor = 0;
   unsigned char rs485ModeEnabled = false;
   unsigned char rs485ActiveHigh = true;
   unsigned char rs485EnableTermination = false;
   unsigned char rs485RxDuringTx = false;
   unsigned char isDtrEnabled = true;
   unsigned char isRtsEnabled = true;
   char xonStartChar = 17;
   char xoffStopChar = 19;

   // Clear any serial port flags and set up raw non-canonical port parameters
   struct termios options = { 0 };
   tcgetattr(serialPortFD, &options);
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

   // Set baud rate and apply changes
   baud_rate baudRateCode = getBaudRateCode(baudRate);
   if (!baudRateCode)
      baudRateCode = B38400;
   cfsetispeed(&options, baudRateCode);
   cfsetospeed(&options, baudRateCode);
   if (tcsetattr(serialPortFD, TCSANOW, &options) || tcsetattr(serialPortFD, TCSANOW, &options))
      return false;

   // Attempt to set the transmit buffer size and any necessary custom baud rates
#if defined(__linux__)

   struct serial_struct serInfo = { 0 };
   if (!ioctl(serialPortFD, TIOCGSERIAL, &serInfo))
   {
      serInfo.xmit_fifo_size = sendDeviceQueueSize;
      serInfo.flags |= ASYNC_LOW_LATENCY;
      ioctl(serialPortFD, TIOCSSERIAL, &serInfo);
   }

   // Attempt to set the requested RS-485 mode
   struct serial_rs485 rs485Conf = { 0 };
   if (!ioctl(serialPortFD, TIOCGRS485, &rs485Conf))
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
      if (ioctl(serialPortFD, TIOCSRS485, &rs485Conf))
         return false;
   }
#endif

   // Configure the serial port read and write timeouts
   return configTimeouts(serialPortFD, timeoutMode, readTimeout, writeTimeout, eventsToMonitor);
}

long openPortNative(void)
{
   const char *portName = comPort;
   unsigned char disableExclusiveLock = false;
   unsigned char disableAutoConfig = false;

   // Try to open existing serial port with read/write access
   int serialPortFD = -1;
   if ((serialPortFD = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC)) > 0)
   {
      // Ensure that multiple root users cannot access the device simultaneously
      if (!disableExclusiveLock && flock(serialPortFD, LOCK_EX | LOCK_NB))
      {
         while (close(serialPortFD) && (errno == EINTR))
            errno = 0;
         serialPortFD = -1;
      }
      else if (!disableAutoConfig && !configPort(serialPortFD))
      {
    	  // Close the port if there was a problem setting the parameters
         fcntl(serialPortFD, F_SETFL, O_NONBLOCK);
         while ((close(serialPortFD) == -1) && (errno == EINTR))
            errno = 0;
         serialPortFD = -1;
      }
   }

   return serialPortFD;
}

long closePortNative(long serialPortFD)
{
	// Force the port to enter non-blocking mode to ensure that any current reads return
	struct termios options = { 0 };
	tcgetattr(serialPortFD, &options);
	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 0;
	fcntl(serialPortFD, F_SETFL, O_NONBLOCK);
	tcsetattr(serialPortFD, TCSANOW, &options);
	tcsetattr(serialPortFD, TCSANOW, &options);

	// Unblock, unlock, and close the port
	fsync(serialPortFD);
	tcdrain(serialPortFD);
	tcflush(serialPortFD, TCIOFLUSH);
	flock(serialPortFD, LOCK_UN | LOCK_NB);
	while (close(serialPortFD) && (errno == EINTR))
		errno = 0;
	   serialPortFD = -1;
	return -1;
}

int readBytes(long serialPortFD, char* buffer, long bytesToRead, long offset, int timeoutMode, int readTimeout)
{
	// Ensure that the allocated read buffer is large enough
	int numBytesRead, numBytesReadTotal = 0, bytesRemaining = bytesToRead, ioctlResult = 0;
	if (bytesToRead > readBufferLength)
	{
		char *newMemory = (char*)realloc(readBuffer, bytesToRead);
		if (!newMemory)
			return -1;
		readBuffer = newMemory;
		readBufferLength = bytesToRead;
	}

	// Infinite blocking mode specified, don't return until we have completely finished the read
	if (((timeoutMode & com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING) > 0) && (readTimeout == 0))
	{
		// While there are more bytes we are supposed to read
		while (bytesRemaining > 0)
		{
			// Attempt to read some number of bytes from the serial port
			do { errno = 0; numBytesRead = read(serialPortFD, readBuffer + numBytesReadTotal, bytesRemaining); } while ((numBytesRead < 0) && (errno == EINTR));
			if ((numBytesRead == -1) || ((numBytesRead == 0) && (ioctl(serialPortFD, FIONREAD, &ioctlResult) == -1)))
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
			do { errno = 0; numBytesRead = read(serialPortFD, readBuffer + numBytesReadTotal, bytesRemaining); } while ((numBytesRead < 0) && (errno == EINTR));
			if ((numBytesRead == -1) || ((numBytesRead == 0) && (ioctl(serialPortFD, FIONREAD, &ioctlResult) == -1)))
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
		do { errno = 0; numBytesRead = read(serialPortFD, readBuffer, bytesToRead); } while ((numBytesRead < 0) && (errno == EINTR));
		if (numBytesRead > 0)
			numBytesReadTotal = numBytesRead;
	}

	// Return number of bytes read if successful
	memcpy(buffer, readBuffer, numBytesReadTotal);
	return (numBytesRead == -1) ? -1 : numBytesReadTotal;
}

void* readingThread(void *arg)
{
   // Read forever in a loop while the port is open
   char readBuffer[2048];
   while (portHandle > 0)
   {
      printf("\nBeginning blocking read...\n");
      int numBytesRead = readBytes(portHandle, readBuffer, sizeof(readBuffer), 0, com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING, 0);
      printf("Read %d bytes\n", numBytesRead);
   }
   return NULL;
}
int testCloseSeparateThread(void)
{
   // Open the serial port
   if ((portHandle = openPortNative()) <= 0)
   {
      printf("ERROR: Could not open port: %s\n", comPort);
      return -1;
   }
   printf("Port opened: %s\n", comPort);

   // Configure the serial port for indefinitely blocking reads
   if (!configTimeouts(portHandle, com_fazecast_jSerialComm_SerialPort_TIMEOUT_READ_BLOCKING, 0, 0, 0))
   {
      printf("ERROR: Could not configure port timeouts\n");
      return -2;
   }
   printf("Blocking read timeouts successfully configured\n");

   // Start a new thread to continuously read from the serial port for 5 seconds
   pthread_t pid;
   if (pthread_create(&pid, NULL, &readingThread, NULL))
   {
      printf("ERROR: Could not create a reading thread\n");
      return -3;
   }
   sleep(5);

   // Close the serial port
   printf("\nAttempting to close serial port from a separate thread...\n");
   if ((portHandle = closePortNative(portHandle)) > 0)
   {
      printf("ERROR: Could not close port: %s\n", comPort);
      return -4;
   }
   printf("Port closed\n");

   // Wait for the reading thread to return
   pthread_join(pid, NULL);
   printf("Reading thread successfully returned\n");
   return 0;
}

int testSimpleOpenClose(void)
{
   // Open the serial port
   if ((portHandle = openPortNative()) <= 0)
   {
      printf("ERROR: Could not open port: %s\n", comPort);
      return -2;
   }
   printf("Port opened\n");

   // Close the serial port
   if ((portHandle = closePortNative(portHandle)) > 0)
   {
      printf("ERROR: Could not close port: %s\n", comPort);
      return -3;
   }
   printf("Port closed\n");
   return 0;
}

int main(int argc, char *argv[])
{
   // Check for correct input parameters
   if (argc != 2)
   {
	  printf("USAGE: ./testOpenClose [PORT_FILE_NAME]\n");
	  return -1;
   }
   comPort = argv[1];

   // Perform one of the above open/close tests
   return testCloseSeparateThread();
   //return testSimpleOpenClose();
}
