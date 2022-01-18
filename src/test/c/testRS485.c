#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/serial.h>
#define termios asmtermios
#define termio asmtermio
#define winsize asmwinsize
#include <asm/ioctls.h>
#include <asm/termios.h>
#undef termio
#undef termios
#undef winsize

int main(int argc, char *argv[])
{
	// Ensure that port handle was passed in
	if (argc != 2)
	{
		printf("Usage: ./testRS485 /dev/port/path\n");
		return 0;
	}
	const char *portName = argv[1];

	// Open serial port
	int portHandle = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
	if (portHandle > 0)
	{
		// Ensure that multiple root users cannot access the device simultaneously
		if (flock(portHandle, LOCK_EX | LOCK_NB))
		{
			while (close(portHandle) && (errno == EINTR))
				errno = 0;
			portHandle = -1;
		}
	}
	if (portHandle <= 0)
	{
		printf("Error opening port at %s\n", portName);
		return -1;
	}

	// Attempt to retrieve RS485 configuration from connected device
	struct serial_rs485 rs485Conf = { 0 };
	int retVal = ioctl(portHandle, TIOCGRS485, &rs485Conf);
	if (retVal)
	{
		printf("Error retrieving RS485 configuration, Code = %d, Errno = %d\n", retVal, errno);
		return -2;
	}

	// Attempt to enable RS485 configuration
	rs485Conf.flags |= SER_RS485_ENABLED;
	rs485Conf.flags |= SER_RS485_RTS_ON_SEND;
	rs485Conf.flags &= ~(SER_RS485_RTS_AFTER_SEND);
	rs485Conf.flags &= ~(SER_RS485_RX_DURING_TX);
	rs485Conf.flags &= ~(SER_RS485_TERMINATE_BUS);
	rs485Conf.delay_rts_before_send = 1;
	rs485Conf.delay_rts_after_send = 1;
	retVal = ioctl(portHandle, TIOCSRS485, &rs485Conf);
	if (retVal)
	{
		printf("Error enabling RS485 configuration, Code = %d, Errno = %d\n", retVal, errno);
		return -3;
	}

	// Attempt to disable RS485 configuration
	rs485Conf.flags &= ~SER_RS485_ENABLED;
	rs485Conf.flags &= ~(SER_RS485_RTS_ON_SEND);
	rs485Conf.flags |= SER_RS485_RTS_AFTER_SEND;
	rs485Conf.flags |= SER_RS485_RX_DURING_TX;
	rs485Conf.flags |= SER_RS485_TERMINATE_BUS;
	retVal = ioctl(portHandle, TIOCSRS485, &rs485Conf);
	if (retVal)
	{
		printf("Error disabling RS485 configuration, Code = %d, Errno = %d\n", retVal, errno);
		return -4;
	}

	// Unblock, unlock, and close the port
	fsync(portHandle);
	tcdrain(portHandle);
	tcflush(portHandle, TCIOFLUSH);
	flock(portHandle, LOCK_UN | LOCK_NB);
	while (close(portHandle) && (errno == EINTR))
		errno = 0;
	portHandle = -1;
	return 0;
}
