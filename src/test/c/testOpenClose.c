#include <errno.h>
#include <fcntl.h>
#include <poll.h>
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
static long portHandle = -1;
static char* comPort = NULL;


// JNI functionality
bool configTimeouts(long serialPortFD, int timeoutMode, int readTimeout, int writeTimeout, int eventsToMonitor)
{
   // Get port timeouts from Java class
   int flags = 0;
   struct termios options = { 0 };
   baud_rate baudRate = 9600;
   tcgetattr(serialPortFD, &options);

   // Set updated port timeouts
   if (((timeoutMode & 0x1) > 0) && (readTimeout > 0)) // Read Semi-blocking with timeout
   {
      options.c_cc[VMIN] = 0;
      options.c_cc[VTIME] = readTimeout / 100;
   }
   else if ((timeoutMode & 0x1) > 0)             // Read Semi-blocking without timeout
   {
      options.c_cc[VMIN] = 1;
      options.c_cc[VTIME] = 0;
   }
   else if (((timeoutMode & 0x10) > 0)  && (readTimeout > 0))   // Read Blocking with timeout
   {
      options.c_cc[VMIN] = 0;
      options.c_cc[VTIME] = readTimeout / 100;
   }
   else if ((timeoutMode & 0x10) > 0)                     // Read Blocking without timeout
   {
      options.c_cc[VMIN] = 1;
      options.c_cc[VTIME] = 0;
   }
   else if ((timeoutMode & 0x1000) > 0)                        // Scanner Mode
   {
      options.c_cc[VMIN] = 1;
      options.c_cc[VTIME] = 1;
   }
   else                                                       // Non-blocking
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

   // Clear any serial port flags and set up raw non-canonical port parameters
   struct termios options = { 0 };
   tcgetattr(serialPortFD, &options);
   options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | INPCK | IGNPAR | IGNCR | ICRNL | IXON | IXOFF);
   options.c_oflag &= ~OPOST;
   options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
   options.c_cflag &= ~(CSIZE | PARENB | CMSPAR | PARODD | CSTOPB | CRTSCTS);

   // Update the user-specified port parameters
   tcflag_t byteSize = (byteSizeInt == 5) ? CS5 : (byteSizeInt == 6) ? CS6 : (byteSizeInt == 7) ? CS7 : CS8;
   tcflag_t parity = (parityInt == 0) ? 0 : (parityInt == 1) ? (PARENB | PARODD) : (parityInt == 2) ? PARENB : (parityInt == 3) ? (PARENB | CMSPAR | PARODD) : (PARENB | CMSPAR);
   options.c_cflag |= (byteSize | parity | CLOCAL | CREAD);
   if (!isDtrEnabled || !isRtsEnabled)
      options.c_cflag &= ~HUPCL;
   if (!rs485ModeEnabled)
      options.c_iflag |= BRKINT;
   if (stopBitsInt == 3)
      options.c_cflag |= CSTOPB;
   if (((flowControl & 0x00000010) > 0) || ((flowControl & 0x00000001) > 0))
      options.c_cflag |= CRTSCTS;
   if (byteSizeInt < 8)
      options.c_iflag |= ISTRIP;
   if (parityInt != 0)
      options.c_iflag |= (INPCK | IGNPAR);
   if ((flowControl & 0x10000) > 0)
      options.c_iflag |= IXOFF;
   if ((flowControl & 0x100000) > 0)
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

bool closePortNative(long serialPortFD)
{
   // Unblock, unlock, and close the port
   fsync(serialPortFD);
   tcdrain(serialPortFD);
   tcflush(serialPortFD, TCIOFLUSH);
   fcntl(serialPortFD, F_SETFL, O_NONBLOCK);
   flock(serialPortFD, LOCK_UN | LOCK_NB);
   while (close(serialPortFD) && (errno == EINTR))
      errno = 0;
   serialPortFD = -1;
   return 0;
}

int testFull(void)
{
   // Open the serial port
   if ((portHandle = openPortNative()) <= 0)
   {
      printf("ERROR: Could not open port: %s\n", comPort);
      return -2;
   }
   printf("Port opened\n");

   // Close the serial port
   closePortNative(portHandle);
   if (portHandle > 0)
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
   return testFull();
}
