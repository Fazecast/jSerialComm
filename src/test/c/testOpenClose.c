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


// OS-specific functionality
#ifndef CMSPAR
#define CMSPAR 010000000000
#endif

#if defined(__linux__)
typedef int baud_rate;
#ifdef __ANDROID__
extern int ioctl(int __fd, int __request, ...);
#else
extern int ioctl(int __fd, unsigned long int __request, ...);
#endif
#elif defined(__APPLE__)
#define fdatasync(a) fsync(a)
#include <termios.h>
typedef speed_t baud_rate;
#endif

#if defined(__linux__)
baud_rate getBaudRateCode(baud_rate baudRate)
{
   // Translate a raw baud rate into a system-specified one
   switch (baudRate)
   {
      case 50:
         return B50;
      case 75:
         return B75;
      case 110:
         return B110;
      case 134:
         return B134;
      case 150:
         return B150;
      case 200:
         return B200;
      case 300:
         return B300;
      case 600:
         return B600;
      case 1200:
         return B1200;
      case 1800:
         return B1800;
      case 2400:
         return B2400;
      case 4800:
         return B4800;
      case 9600:
         return B9600;
      case 19200:
         return B19200;
      case 38400:
         return B38400;
      case 57600:
#ifdef B57600
         return B57600;
#else
         return 0;
#endif
      case 115200:
#ifdef B115200
         return B115200;
#else
         return 0;
#endif
      case 230400:
#ifdef B230400
         return B230400;
#else
         return 0;
#endif
      case 460800:
#ifdef B460800
         return B460800;
#else
         return 0;
#endif
      case 500000:
#ifdef B500000
         return B500000;
#else
         return 0;
#endif
      case 576000:
#ifdef B576000
         return B576000;
#else
         return 0;
#endif
      case 921600:
#ifdef B921600
         return B921600;
#else
         return 0;
#endif
      default:
         return 0;
   }

   return 0;
}
int setBaudRateCustom(int portFD, baud_rate baudRate)
{
#ifdef TCSETS2
   struct termios2 options = { 0 };
   ioctl(portFD, TCGETS2, &options);
   options.c_cflag &= ~CBAUD;
   options.c_cflag |= BOTHER;
   options.c_ispeed = baudRate;
   options.c_ospeed = baudRate;
   int retVal = ioctl(portFD, TCSETS2, &options);
#else
   struct serial_struct serInfo;
   int retVal = ioctl(portFD, TIOCGSERIAL, &serInfo);
   if (retVal == 0)
   {
      serInfo.flags &= ~ASYNC_SPD_MASK;
      serInfo.flags |= ASYNC_SPD_CUST | ASYNC_LOW_LATENCY;
      serInfo.custom_divisor = serInfo.baud_base / baudRate;
      if (sersInfo.custom_divisor == 0)
         serInfo.custom_divisor = 1;
      retVal = ioctl(portFD, TIOCSSERIAL, &serInfo);
   }
#endif
   return (retVal == 0);
}
#elif defined(__APPLE__)
baud_rate getBaudRateCode(baud_rate baudRate)
{
   // Translate a raw baud rate into a system-specified one
   switch (baudRate)
   {
      case 50:
         return B50;
      case 75:
         return B75;
      case 110:
         return B110;
      case 134:
         return B134;
      case 150:
         return B150;
      case 200:
         return B200;
      case 300:
         return B300;
      case 600:
         return B600;
      case 1200:
         return B1200;
      case 1800:
         return B1800;
      case 2400:
         return B2400;
      case 4800:
         return B4800;
      case 9600:
         return B9600;
      case 19200:
         return B19200;
      case 38400:
         return B38400;
      default:
         return 0;
   }

   return 0;
}
int setBaudRateCustom(int portFD, baud_rate baudRate)
{
   // Use OSX-specific ioctls to set a custom baud rate
   unsigned long microseconds = 1000;
   int retVal = ioctl(portFD, IOSSIOSPEED, &baudRate);
   ioctl(portFD, IOSSDATALAT, &microseconds);
   return (retVal == 0);
}
#endif


// Global static variables
static long portHandle = -1;
static char* comPort = NULL;


// JNI functionality
bool configTimeouts(long serialPortFD)
{
   // Get port timeouts from Java class
   if (serialPortFD <= 0)
      return false;
   baud_rate baudRate = 9600;
   baud_rate baudRateCode = getBaudRateCode(baudRate);
   int timeoutMode = 0;
   int readTimeout = 0;

   // Retrieve existing port configuration
   struct termios options = {0};
   tcgetattr(serialPortFD, &options);
   int flags = fcntl(serialPortFD, F_GETFL);
   if (flags == -1)
      return false;

   // Set updated port timeouts
   if (((timeoutMode & 0x1) > 0) && (readTimeout > 0)) // Read Semi-blocking with timeout
   {
      flags &= ~O_NONBLOCK;
      options.c_cc[VMIN] = 0;
      options.c_cc[VTIME] = readTimeout / 100;
   }
   else if ((timeoutMode & 0x1) > 0)             // Read Semi-blocking without timeout
   {
      flags &= ~O_NONBLOCK;
      options.c_cc[VMIN] = 1;
      options.c_cc[VTIME] = 0;
   }
   else if (((timeoutMode & 0x10) > 0)  && (readTimeout > 0))   // Read Blocking with timeout
   {
      flags &= ~O_NONBLOCK;
      options.c_cc[VMIN] = 0;
      options.c_cc[VTIME] = readTimeout / 100;
   }
   else if ((timeoutMode & 0x10) > 0)                     // Read Blocking without timeout
   {
      flags &= ~O_NONBLOCK;
      options.c_cc[VMIN] = 1;
      options.c_cc[VTIME] = 0;
   }
   else if ((timeoutMode & 0x1000) > 0)                        // Scanner Mode
   {
      flags &= ~O_NONBLOCK;
      options.c_cc[VMIN] = 1;
      options.c_cc[VTIME] = 1;
   }
   else                                                       // Non-blocking
   {
      flags |= O_NONBLOCK;
      options.c_cc[VMIN] = 0;
      options.c_cc[VTIME] = 0;
   }

   // Apply changes
   int retVal = fcntl(serialPortFD, F_SETFL, flags);
   if (retVal != -1)
      retVal = tcsetattr(serialPortFD, TCSANOW, &options);
   if (baudRateCode == 0)
      setBaudRateCustom(serialPortFD, baudRate);
   return ((retVal == 0) ? true : false);
}

bool configPort(long serialPortFD)
{
   if (serialPortFD <= 0)
      return false;
   struct termios options = {0};

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
   unsigned char configDisabled = false;
   unsigned char rs485ModeEnabled = false;
   unsigned char rs485ActiveHigh = true;
   unsigned char isDtrEnabled = true;
   unsigned char isRtsEnabled = true;
   tcflag_t byteSize = (byteSizeInt == 5) ? CS5 : (byteSizeInt == 6) ? CS6 : (byteSizeInt == 7) ? CS7 : CS8;
   tcflag_t parity = (parityInt == 0) ? 0 : (parityInt == 1) ? (PARENB | PARODD) : (parityInt == 2) ? PARENB : (parityInt == 3) ? (PARENB | CMSPAR | PARODD) : (PARENB | CMSPAR);
   tcflag_t XonXoffInEnabled = ((flowControl & 0x10000) > 0) ? IXOFF : 0;
   tcflag_t XonXoffOutEnabled = ((flowControl & 0x100000) > 0) ? IXON : 0;

   // Set updated port parameters
   tcgetattr(serialPortFD, &options);
   options.c_cflag &= ~(CSIZE | PARENB | CMSPAR | PARODD);
   options.c_cflag |= (byteSize | parity | CLOCAL | CREAD);
   if (stopBitsInt == 3)
      options.c_cflag |= CSTOPB;
   else
      options.c_cflag &= ~CSTOPB;
   if (((flowControl & 0x00000010) > 0) || ((flowControl & 0x00000001) > 0))
      options.c_cflag |= CRTSCTS;
   else
      options.c_cflag &= ~CRTSCTS;
   if (!isDtrEnabled || !isRtsEnabled)
      options.c_cflag &= ~HUPCL;
   options.c_iflag &= ~(INPCK | IGNPAR | PARMRK | ISTRIP);
   if (byteSizeInt < 8)
      options.c_iflag |= ISTRIP;
   if (parityInt != 0)
      options.c_iflag |= (INPCK | IGNPAR);
   options.c_iflag |= (XonXoffInEnabled | XonXoffOutEnabled);

   // Set baud rate and apply changes
   baud_rate baudRateCode = getBaudRateCode(baudRate);
   unsigned char nonStandardBaudRate = (baudRateCode == 0);
   if (nonStandardBaudRate)
      baudRateCode = B38400;
   cfsetispeed(&options, baudRateCode);
   cfsetospeed(&options, baudRateCode);
   int retVal = configDisabled ? 0 : tcsetattr(serialPortFD, TCSANOW, &options);

   // Attempt to set the transmit buffer size and any necessary custom baud rates
#if defined(__linux__)
   struct serial_struct serInfo = {0};
   if (ioctl(serialPortFD, TIOCGSERIAL, &serInfo) == 0)
   {
      serInfo.xmit_fifo_size = sendDeviceQueueSize;
      ioctl(serialPortFD, TIOCSSERIAL, &serInfo);
   }
#else
   sendDeviceQueueSize = sysconf(_SC_PAGESIZE);
#endif
   receiveDeviceQueueSize, sysconf(_SC_PAGESIZE);
   if (nonStandardBaudRate)
      setBaudRateCustom(serialPortFD, baudRate);

   // Attempt to set the requested RS-485 mode
#if defined(__linux__)
   struct serial_rs485 rs485Conf = {0};
   if (ioctl(serialPortFD, TIOCGRS485, &rs485Conf) == 0)
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
      rs485Conf.delay_rts_before_send = rs485DelayBefore;
      rs485Conf.delay_rts_after_send = rs485DelayAfter;
      ioctl(serialPortFD, TIOCSRS485, &rs485Conf);
   }
#endif
   return ((retVal == 0) && configTimeouts(serialPortFD) ? true : false);
}

long openPortNative(void)
{
   const char *portName = comPort;
   unsigned char isDtrEnabled = true;
   unsigned char isRtsEnabled = true;

   // Try to open existing serial port with read/write access
   int serialPortFD = -1;
   if ((serialPortFD = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK)) > 0)
   {
      // Ensure that multiple root users cannot access the device simultaneously
      if (flock(serialPortFD, LOCK_EX | LOCK_NB) == -1)
      {
         tcdrain(serialPortFD);
         while ((close(serialPortFD) == -1) && (errno == EINTR))
            errno = 0;
         serialPortFD = -1;
      }
      else
      {
         // Clear any serial port flags and set up raw, non-canonical port parameters
         struct termios options = {0};
         fcntl(serialPortFD, F_SETFL, 0);
         tcgetattr(serialPortFD, &options);
         cfmakeraw(&options);
         if (!isDtrEnabled || !isRtsEnabled)
            options.c_cflag &= ~HUPCL;
         options.c_iflag |= BRKINT;
         tcsetattr(serialPortFD, TCSANOW, &options);

         // Configure the port parameters and timeouts
         if (configPort(serialPortFD))
            portHandle = serialPortFD;
         else
         {
            // Close the port if there was a problem setting the parameters
            tcdrain(serialPortFD);
            while ((close(serialPortFD) == -1) && (errno == EINTR))
               errno = 0;
            serialPortFD = -1;
         }
      }
   }

   return serialPortFD;
}

bool closePortNative(long serialPortFD)
{
   // Ensure that the port is open
   if (serialPortFD <= 0)
      return true;

   // Force the port to enter non-blocking mode to ensure that any current reads return
   struct termios options = {0};
   tcgetattr(serialPortFD, &options);
   int flags = fcntl(serialPortFD, F_GETFL);
   flags |= O_NONBLOCK;
   options.c_cc[VMIN] = 0;
   options.c_cc[VTIME] = 0;
   int retVal = fcntl(serialPortFD, F_SETFL, flags);
   tcsetattr(serialPortFD, TCSANOW, &options);
   tcdrain(serialPortFD);

   // Close the port
   int closeResult = 0;
   flock(serialPortFD, LOCK_UN | LOCK_NB);
   fdatasync(serialPortFD);
   while (((closeResult = close(serialPortFD)) == -1) && (errno == EINTR))
   {
      printf("CLOSE INTERRUPTED, ERROR CODE: %d\n", closeResult);
      errno = 0;
   }
   if (closeResult == -1)
      printf("CLOSE FAILED, ERROR CODE: %d\n", closeResult);
   portHandle = -1l;
   return true;
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
