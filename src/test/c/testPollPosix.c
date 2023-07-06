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
#include "PosixHelperFunctions.h"

static serialPortVector serialPorts = { NULL, 0, 0 };
static serialPort *currentPort = NULL;

#define PERR 0b001000000000
#define PHUP 0b000100000000
#define PIN 0b000010000000
#define PNVAL 0b000001000000
#define POUT 0b000000100000
#define PPRI 0b000000010000
#define PRDBAND 0b000000001000
#define PRDNORM 0b000000000100
#define PWRBAND 0b000000000010
#define PWRNORM 0b000000000001

unsigned short convertEventCodes(short revents)
{
	unsigned short eventBits = 0;
	if (revents & POLLERR)
		eventBits |= PERR;
	if (revents & POLLHUP)
		eventBits |= PHUP;
	if (revents & POLLIN)
		eventBits |= PIN;
	if (revents & POLLNVAL)
		eventBits |= PNVAL;
	if (revents & POLLOUT)
		eventBits |= POUT;
	if (revents & POLLPRI)
		eventBits |= PPRI;
	if (revents & POLLRDBAND)
		eventBits |= PRDBAND;
	if (revents & POLLRDNORM)
		eventBits |= PRDNORM;
	if (revents & POLLWRBAND)
		eventBits |= PWRBAND;
	if (revents & POLLWRNORM)
		eventBits |= PWRNORM;
	return eventBits;
}

void ctrlCHandler(int dummy)
{
	if (currentPort)
		currentPort->eventListenerRunning = 0;
	else
	{
		printf("\n");
		exit(0);
	}
}

int configPort(serialPort *port)
{
	// Clear any serial port flags and set up raw non-canonical port parameters
	struct termios options = { 0 };
	tcgetattr(port->handle, &options);
	options.c_cc[VSTART] = (unsigned char)17;
	options.c_cc[VSTOP] = (unsigned char)19;
	options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | INPCK | IGNPAR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
	options.c_oflag &= ~OPOST;
	options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	options.c_cflag &= ~(CSIZE | PARENB | CMSPAR | PARODD | CSTOPB | CRTSCTS);

	// Update the user-specified port parameters
	int baudRate = 115200;
	tcflag_t byteSize = CS8;
	tcflag_t parity = 0;
	options.c_cflag |= (byteSize | parity | CLOCAL | CREAD);
	options.c_cflag &= ~HUPCL;

	// Configure the serial port read and write timeouts
	int flags = 0;
	port->eventsMask = com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PORT_DISCONNECTED;// | com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE;
	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 10;

	// Apply changes
	if (fcntl(port->handle, F_SETFL, flags))
		return 0;
	if (setConfigOptions(port->handle, baudRate, &options))
		return 0;
	return 1;
}

int waitForEvent(serialPort *port)
{
	// Initialize local variables
	int pollResult;
	short pollEventsMask = ((port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE) || (port->eventsMask & com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_RECEIVED)) ? (POLLIN | POLLERR) : (POLLHUP | POLLERR);
	jint event = com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_TIMED_OUT;
	struct pollfd waitingSet = { port->handle, pollEventsMask, 0 };

	// Wait for a serial port event
	do
	{
		waitingSet.revents = 0;
		pollResult = poll(&waitingSet, 1, 1000);
		printf("Poll Result: %d, Revents: %hu, Codes: %hu\n", pollResult, waitingSet.revents, convertEventCodes(waitingSet.revents));
	}
	while ((pollResult == 0) && port->eventListenerRunning);

	// Return the detected port events
	if (waitingSet.revents & (POLLHUP | POLLNVAL))
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PORT_DISCONNECTED;
	else if (waitingSet.revents & POLLIN)
		event |= com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE;
	return event;
}

int main(void)
{
	// Enumerate all serial ports
	signal(SIGINT, ctrlCHandler);
	searchForComPorts(&serialPorts);

	// Prompt user which port to open
	int userSelection = -1;
	while ((userSelection < 0) || (userSelection >= serialPorts.length))
	{
		printf("Select the index of the serial device to connect to:\n\n");
		for (int i = 0; i < serialPorts.length; ++i)
		{
			serialPort *port = serialPorts.ports[i];
			printf("\t[%d]: %s (Description = %s)\n", i, port->portPath, port->portDescription);
		}
		printf("\nTarget device index: ");
		scanf("%d", &userSelection);
	}
	serialPort *port = serialPorts.ports[userSelection];

	// Try to open the serial port with read/write access
	int portHandle = open(port->portPath, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
	if (portHandle > 0)
	{
		// Set the newly opened port handle in the serial port structure
		port->handle = portHandle;

		// Quickly set the desired RTS/DTR line status immediately upon opening
		int modemBits = TIOCM_DTR;
		ioctl(port->handle, TIOCMBIS, &modemBits);
		modemBits = TIOCM_RTS;
		ioctl(port->handle, TIOCMBIS, &modemBits);

		// Ensure that multiple root users cannot access the device simultaneously
		if (flock(port->handle, LOCK_EX | LOCK_NB))
		{
			while (close(port->handle) && (errno == EINTR))
				errno = 0;
			port->handle = -1;
		}
		else if (!configPort(port))
		{
			// Close the port if there was a problem setting the parameters
			fcntl(port->handle, F_SETFL, O_NONBLOCK);
			while (close(port->handle) && (errno == EINTR))
				errno = 0;
			port->handle = -1;
		}

		// Start listening for events
		currentPort = port;
		port->eventListenerRunning = 1;
		while (port->eventListenerRunning)
		{
			int event = waitForEvent(port);
			if (event != com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_TIMED_OUT)
				printf("Received event: %s\n", event == com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PORT_DISCONNECTED ? "Disconnected" : "Available");
			if (event == com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_PORT_DISCONNECTED)
				port->eventListenerRunning = 0;
			else if (event == com_fazecast_jSerialComm_SerialPort_LISTENING_EVENT_DATA_AVAILABLE)
				tcflush(port->handle, TCIOFLUSH);
		}

		// Close the port
		struct termios options = { 0 };
		tcgetattr(port->handle, &options);
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = 0;
		fcntl(port->handle, F_SETFL, O_NONBLOCK);
		tcsetattr(port->handle, TCSANOW, &options);
		fdatasync(port->handle);
		tcflush(port->handle, TCIOFLUSH);
		flock(port->handle, LOCK_UN | LOCK_NB);
		while (close(port->handle) && (errno == EINTR))
			errno = 0;
		port->handle = -1;
	}

	// Clean up all memory and return
	cleanUpVector(&serialPorts);
	return 0;
}
