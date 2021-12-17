#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "PosixHelperFunctions.h"

static serialPortVector comPorts = { NULL, 0, 0 };

#if defined(__linux__)

#include <linux/serial.h>
#include <asm/termios.h>
#include <asm/ioctls.h>

void getDriverNameTest(const char* directoryToSearch, char* friendlyName)
{
	friendlyName[0] = '\0';

	// Open the directory
	DIR *directoryIterator = opendir(directoryToSearch);
	if (!directoryIterator)
		return;

	// Read all sub-directories in the current directory
	struct dirent *directoryEntry = readdir(directoryIterator);
	while (directoryEntry)
	{
		// Check if entry is a valid sub-directory
		if (directoryEntry->d_name[0] != '.')
		{
			// Get the readable part of the driver name
			strcpy(friendlyName, "USB-to-Serial Port (");
			char *startingPoint = strchr(directoryEntry->d_name, ':');
			if (startingPoint != NULL)
				strcat(friendlyName, startingPoint+1);
			else
				strcat(friendlyName, directoryEntry->d_name);
			strcat(friendlyName, ")");
			break;
		}
		directoryEntry = readdir(directoryIterator);
	}

	// Close the directory
	closedir(directoryIterator);
}

void getFriendlyNameTest(const char* productFile, char* friendlyName)
{
	int friendlyNameLength = 0;
	friendlyName[0] = '\0';

	FILE *input = fopen(productFile, "rb");
	if (input)
	{
		char ch = getc(input);
		while ((ch != '\n') && (ch != EOF))
		{
			friendlyName[friendlyNameLength++] = ch;
			ch = getc(input);
		}
		friendlyName[friendlyNameLength] = '\0';
		fclose(input);
	}
}

void getInterfaceDescriptionTest(const char* interfaceFile, char* interfaceDescription)
{
	int interfaceDescriptionLength = 0;
	interfaceDescription[0] = '\0';

	FILE *input = fopen(interfaceFile, "rb");
	if (input)
	{
		char ch = getc(input);
		while ((ch != '\n') && (ch != EOF))
		{
			interfaceDescription[interfaceDescriptionLength++] = ch;
			ch = getc(input);
		}
		interfaceDescription[interfaceDescriptionLength] = '\0';
		fclose(input);
	}
}

char getPortLocationTest(const char* portDirectory, char* portLocation)
{
	// Set location of busnum and devpath files
	char isUSB = 1;
	char* busnumFile = (char*)malloc(strlen(portDirectory) + 16);
	strcpy(busnumFile, portDirectory);
	strcat(busnumFile, "/busnum");
	char* devpathFile = (char*)malloc(strlen(portDirectory) + 16);
	strcpy(devpathFile, portDirectory);
	strcat(devpathFile, "/devpath");
	int portLocationLength = 0;
	portLocation[0] = '\0';

	// Read the bus number
	FILE *input = fopen(busnumFile, "rb");
	if (input)
	{
		char ch = getc(input);
		while ((ch != '\n') && (ch != EOF))
		{
			portLocation[portLocationLength++] = ch;
			ch = getc(input);
		}
		portLocation[portLocationLength++] = '-';
		fclose(input);
	}
	else
	{
		isUSB = 0;
		portLocation[portLocationLength++] = '0';
		portLocation[portLocationLength++] = '-';
	}

	// Read the device path
	input = fopen(devpathFile, "rb");
	if (input)
	{
		char ch = getc(input);
		while ((ch != '\n') && (ch != EOF))
		{
			portLocation[portLocationLength++] = ch;
			ch = getc(input);
		}
		portLocation[portLocationLength] = '\0';
		fclose(input);
	}
	else
	{
		isUSB = 0;
		portLocation[portLocationLength++] = '0';
	}

	// Clean up the dynamic memory
	free(devpathFile);
	free(busnumFile);
	return isUSB;
}

char driverGetPortLocationTest(char topLevel, const char *fullPathToSearch, const char *deviceName, char* portLocation, char searchBackwardIteration)
{
	// Open the linux USB device directory
	char isUSB = 0;
	DIR *directoryIterator = opendir(fullPathToSearch);
	if (!directoryIterator)
		return isUSB;

	if (!searchBackwardIteration)
	{
		// Read all sub-directories in the current directory
		struct dirent *directoryEntry = readdir(directoryIterator);
		while (directoryEntry && !isUSB)
		{
			// Check if entry is a sub-directory
			if ((topLevel || (directoryEntry->d_type == DT_DIR)) && (directoryEntry->d_name[0] != '.'))
			{
				// Set up the next directory to search
				char* nextDirectory = (char*)malloc(strlen(fullPathToSearch) + strlen(directoryEntry->d_name) + 5);
				strcpy(nextDirectory, fullPathToSearch);
				strcat(nextDirectory, directoryEntry->d_name);

				// Only process directories that match the device name
				if (strcmp(directoryEntry->d_name, deviceName) == 0)
				{
					strcat(nextDirectory, "/..");
					isUSB = driverGetPortLocationTest(0, nextDirectory, deviceName, portLocation, 1);
				}
				else
				{
					// Search for more serial ports within the directory
					strcat(nextDirectory, "/");
					isUSB = driverGetPortLocationTest(0, nextDirectory, deviceName, portLocation, 0);
				}
				free(nextDirectory);
			}
			directoryEntry = readdir(directoryIterator);
		}
	}
	else
	{
		// Read all files in the current directory
		char hasBusnum = 0, hasDevpath = 0;
		struct dirent *directoryEntry = readdir(directoryIterator);
		while (directoryEntry)
		{
			// Check if entry is a regular file with the expected name
			if (directoryEntry->d_type == DT_REG)
			{
				if (strcmp(directoryEntry->d_name, "busnum") == 0)
					hasBusnum = 1;
				else if (strcmp(directoryEntry->d_name, "devpath") == 0)
					hasDevpath = 1;
			}
			directoryEntry = readdir(directoryIterator);
		}

		// Check if the current directory has the required information files
		if ((!hasBusnum || !hasDevpath || !(isUSB = getPortLocationTest(fullPathToSearch, portLocation))) && (searchBackwardIteration < 10))
		{
			char* nextDirectory = (char*)malloc(strlen(fullPathToSearch) + 5);
			strcpy(nextDirectory, fullPathToSearch);
			strcat(nextDirectory, "/..");
			isUSB = driverGetPortLocationTest(0, nextDirectory, deviceName, portLocation, searchBackwardIteration + 1);
			free(nextDirectory);
		}
	}

	// Close the directory
	closedir(directoryIterator);
	return isUSB;
}

void recursiveSearchForComPortsTest(serialPortVector* comPorts, const char* fullPathToSearch)
{
	// Open the directory
	DIR *directoryIterator = opendir(fullPathToSearch);
	if (!directoryIterator)
		return;

	// Read all sub-directories in the current directory
	struct dirent *directoryEntry = readdir(directoryIterator);
	while (directoryEntry)
	{
		// Check if entry is a sub-directory
		if (directoryEntry->d_type == DT_DIR)
		{
			// Only process non-dot, non-virtual directories
			if ((directoryEntry->d_name[0] != '.') && (strcmp(directoryEntry->d_name, "virtual") != 0))
			{
				// See if the directory names a potential serial port
				if ((strlen(directoryEntry->d_name) > 3) &&
						(((directoryEntry->d_name[0] == 't') && (directoryEntry->d_name[1] == 't') && (directoryEntry->d_name[2] == 'y')) ||
								((directoryEntry->d_name[0] == 'r') && (directoryEntry->d_name[1] == 'f') && (directoryEntry->d_name[2] == 'c'))))
				{
					// Determine system name of port
					char* systemName = (char*)malloc(256);
					strcpy(systemName, "/dev/");
					strcat(systemName, directoryEntry->d_name);

					// Check if port is already enumerated
					serialPort *port = fetchPort(comPorts, systemName);
					if (port)
						port->enumerated = 1;
					else
					{
						// See if device has a registered friendly name
						char* portLocation = (char*)malloc(128);
						char* friendlyName = (char*)malloc(256);
						char* productFile = (char*)malloc(strlen(fullPathToSearch) + strlen(directoryEntry->d_name) + 30);
						strcpy(productFile, fullPathToSearch);
						strcat(productFile, directoryEntry->d_name);
						strcat(productFile, "/device/..");
						char isUSB = getPortLocationTest(productFile, portLocation);
						if (!isUSB)
							isUSB = driverGetPortLocationTest(1, "/sys/bus/usb/devices/", directoryEntry->d_name, portLocation, 0);
						strcat(productFile, "/product");
						getFriendlyNameTest(productFile, friendlyName);
						if (friendlyName[0] == '\0')		// Must be a physical (or emulated) port
						{
							// See if this is a USB-to-Serial converter based on the driver loaded
							strcpy(productFile, fullPathToSearch);
							strcat(productFile, directoryEntry->d_name);
							strcat(productFile, "/driver/module/drivers");
							getDriverNameTest(productFile, friendlyName);
							if (friendlyName[0] == '\0')	// Must be a physical port
							{
								// Ensure that the platform port is actually open
								struct serial_struct serialInfo = { 0 };
								int fd = open(systemName, O_RDWR | O_NONBLOCK | O_NOCTTY);
								if (fd >= 0)
								{
									if ((strlen(directoryEntry->d_name) >= 6) && (directoryEntry->d_name[0] == 'r') && (directoryEntry->d_name[1] == 'f') && (directoryEntry->d_name[2] == 'c') &&
											(directoryEntry->d_name[3] == 'o') && (directoryEntry->d_name[4] == 'm') && (directoryEntry->d_name[5] == 'm'))
									{
										strcpy(friendlyName, "Bluetooth Port ");
										strcat(friendlyName, directoryEntry->d_name);
										pushBack(comPorts, systemName, friendlyName, friendlyName, portLocation);
									}
									else if (((strlen(directoryEntry->d_name) >= 6) && (directoryEntry->d_name[3] == 'A') && (directoryEntry->d_name[4] == 'M') && (directoryEntry->d_name[5] == 'A')) ||
											((ioctl(fd, TIOCGSERIAL, &serialInfo) == 0) && (serialInfo.type != PORT_UNKNOWN)))
									{
										strcpy(friendlyName, "Physical Port ");
										strcat(friendlyName, directoryEntry->d_name+3);
										pushBack(comPorts, systemName, friendlyName, friendlyName, portLocation);
									}
									close(fd);
								}
							}
							else
							{
								// Attempt to read from the device interface file
								char* interfaceDescription = (char*)malloc(256);
								char* interfaceFile = (char*)malloc(strlen(fullPathToSearch) + strlen(directoryEntry->d_name) + 30);
								strcpy(interfaceFile, fullPathToSearch);
								strcat(interfaceFile, directoryEntry->d_name);
								strcat(interfaceFile, "/../interface");
								getInterfaceDescriptionTest(interfaceFile, interfaceDescription);
								if (interfaceDescription[0] == '\0')
								{
									strcpy(interfaceFile, fullPathToSearch);
									strcat(interfaceFile, directoryEntry->d_name);
									strcat(interfaceFile, "/device/../interface");
									getInterfaceDescriptionTest(interfaceFile, interfaceDescription);
								}
								if (interfaceDescription[0] == '\0')
									strcpy(interfaceDescription, friendlyName);
								pushBack(comPorts, systemName, friendlyName, interfaceDescription, portLocation);

								// Clean up memory
								free(interfaceFile);
								free(interfaceDescription);
							}
						}
						else
						{
							// Attempt to read from the device interface file
							char* interfaceDescription = (char*)malloc(256);
							char* interfaceFile = (char*)malloc(strlen(fullPathToSearch) + strlen(directoryEntry->d_name) + 30);
							strcpy(interfaceFile, fullPathToSearch);
							strcat(interfaceFile, directoryEntry->d_name);
							strcat(interfaceFile, "/../interface");
							getInterfaceDescriptionTest(interfaceFile, interfaceDescription);
							if (interfaceDescription[0] == '\0')
							{
								strcpy(interfaceFile, fullPathToSearch);
								strcat(interfaceFile, directoryEntry->d_name);
								strcat(interfaceFile, "/device/../interface");
								getInterfaceDescriptionTest(interfaceFile, interfaceDescription);
							}
							if (interfaceDescription[0] == '\0')
								strcpy(interfaceDescription, friendlyName);
							pushBack(comPorts, systemName, friendlyName, interfaceDescription, portLocation);

							// Clean up memory
							free(interfaceFile);
							free(interfaceDescription);
						}

						// Clean up memory
						free(productFile);
						free(friendlyName);
						free(portLocation);
					}

					// Clean up memory
					free(systemName);
				}
				else
				{
					// Search for more serial ports within the directory
					char* nextDirectory = (char*)malloc(strlen(fullPathToSearch) + strlen(directoryEntry->d_name) + 5);
					strcpy(nextDirectory, fullPathToSearch);
					strcat(nextDirectory, directoryEntry->d_name);
					strcat(nextDirectory, "/");
					recursiveSearchForComPortsTest(comPorts, nextDirectory);
					free(nextDirectory);
				}
			}
		}
		directoryEntry = readdir(directoryIterator);
	}

	// Close the directory
	closedir(directoryIterator);
}

void driverBasedSearchForComPortsTest(serialPortVector* comPorts, const char* fullPathToDriver, const char* fullBasePathToPort)
{
	// Search for unidentified physical serial ports
	FILE *serialDriverFile = fopen(fullPathToDriver, "rb");
	if (serialDriverFile)
	{
		char* serialLine = (char*)malloc(128);
		while (fgets(serialLine, 128, serialDriverFile))
			if (strstr(serialLine, "uart:") && (strstr(serialLine, "uart:unknown") == NULL))
			{
				// Determine system name of port
				*strchr(serialLine, ':') = '\0';
				char* systemName = (char*)malloc(256);
				strcpy(systemName, fullBasePathToPort);
				strcat(systemName, serialLine);

				// Check if port is already enumerated
				serialPort *port = fetchPort(comPorts, systemName);
				if (port)
					port->enumerated = 1;
				else
				{
					// Ensure that the port is valid and not a symlink
					struct stat fileStats;
					if ((access(systemName, F_OK) == 0) && (lstat(systemName, &fileStats) == 0) && !S_ISLNK(fileStats.st_mode))
					{
						// Set static friendly name
						char* friendlyName = (char*)malloc(256);
						strcpy(friendlyName, "Physical Port ");
						strcat(friendlyName, serialLine);

						// Add the port to the list
						pushBack(comPorts, systemName, friendlyName, friendlyName, "0-0", 0);

						// Clean up memory
						free(friendlyName);
					}
				}

				// Clean up memory
				free(systemName);
			}
		free(serialLine);
		fclose(serialDriverFile);
	}
}

void lastDitchSearchForComPortsTest(serialPortVector* comPorts)
{
	// Open the linux dev directory
	DIR *directoryIterator = opendir("/dev/");
	if (!directoryIterator)
		return;

	// Read all files in the current directory
	struct dirent *directoryEntry = readdir(directoryIterator);
	while (directoryEntry)
	{
		// See if the file names a potential serial port
		if ((strlen(directoryEntry->d_name) >= 6) && (directoryEntry->d_name[0] == 't') && (directoryEntry->d_name[1] == 't') && (directoryEntry->d_name[2] == 'y') &&
				(((directoryEntry->d_name[3] == 'A') && (directoryEntry->d_name[4] == 'M') && (directoryEntry->d_name[5] == 'A')) ||
						((directoryEntry->d_name[3] == 'A') && (directoryEntry->d_name[4] == 'C') && (directoryEntry->d_name[5] == 'M')) ||
						((directoryEntry->d_name[3] == 'U') && (directoryEntry->d_name[4] == 'S') && (directoryEntry->d_name[5] == 'B'))))
		{
			// Determine system name of port
			char* systemName = (char*)malloc(256);
			strcpy(systemName, "/dev/");
			strcat(systemName, directoryEntry->d_name);

			// Check if port is already enumerated
			serialPort *port = fetchPort(comPorts, systemName);
			if (port)
				port->enumerated = 1;
			else
			{
				// Set static friendly name
				char* friendlyName = (char*)malloc(256);
				strcpy(friendlyName, "USB-Based Serial Port");

				// Add the port to the list
				pushBack(comPorts, systemName, friendlyName, friendlyName, "0-0", 1);

				// Clean up memory
				free(friendlyName);
			}

			// Clean up memory
			free(systemName);
		}
		else if ((strlen(directoryEntry->d_name) >= 6) && (directoryEntry->d_name[0] == 't') && (directoryEntry->d_name[1] == 't') && (directoryEntry->d_name[2] == 'y') &&
				(directoryEntry->d_name[3] == 'A') && (directoryEntry->d_name[4] == 'P'))
		{
			// Determine system name of port
			char* systemName = (char*)malloc(256);
			strcpy(systemName, "/dev/");
			strcat(systemName, directoryEntry->d_name);

			// Check if port is already enumerated
			serialPort *port = fetchPort(comPorts, systemName);
			if (port)
				port->enumerated = 1;
			else
			{
				// Set static friendly name
				char* friendlyName = (char*)malloc(256);
				strcpy(friendlyName, "Advantech Extended Serial Port");

				// Add the port to the list
				pushBack(comPorts, systemName, friendlyName, friendlyName, "0-0", 0);

				// Clean up memory
				free(friendlyName);
			}

			// Clean up memory
			free(systemName);
		}
		else if ((strlen(directoryEntry->d_name) >= 6) && (directoryEntry->d_name[0] == 'r') && (directoryEntry->d_name[1] == 'f') && (directoryEntry->d_name[2] == 'c') &&
				(directoryEntry->d_name[3] == 'o') && (directoryEntry->d_name[4] == 'm') && (directoryEntry->d_name[5] == 'm'))
		{
			// Determine system name of port
			char* systemName = (char*)malloc(256);
			strcpy(systemName, "/dev/");
			strcat(systemName, directoryEntry->d_name);

			// Check if port is already enumerated
			serialPort *port = fetchPort(comPorts, systemName);
			if (port)
				port->enumerated = 1;
			else
			{
				// Set static friendly name
				char* friendlyName = (char*)malloc(256);
				strcpy(friendlyName, "Bluetooth-Based Serial Port");

				// Add the port to the list
				pushBack(comPorts, systemName, friendlyName, friendlyName, "0-0", 0);

				// Clean up memory
				free(friendlyName);
			}

			// Clean up memory
			free(systemName);
		}
		directoryEntry = readdir(directoryIterator);
	}

	// Close the directory
	closedir(directoryIterator);
}

#elif defined(__FreeBSD__)

void searchForComPorts(serialPortVector* comPorts)
{
	// Open the FreeBSD dev directory
	DIR *directoryIterator = opendir("/dev/");
	if (directoryIterator)
	{
		// Read all files in the current directory
		struct dirent *directoryEntry = readdir(directoryIterator);
		while (directoryEntry)
		{
			// See if the file names a potential serial port
			if ((strlen(directoryEntry->d_name) >= 4) && (directoryEntry->d_name[0] != '.') &&
					(((directoryEntry->d_name[0] == 't') && (directoryEntry->d_name[1] == 't') && (directoryEntry->d_name[2] == 'y') && (directoryEntry->d_name[3] != 'v')) ||
					 ((directoryEntry->d_name[0] == 'c') && (directoryEntry->d_name[1] == 'u') && (directoryEntry->d_name[2] == 'a'))))
			{
				// Ensure that the file is not an init or a lock file
				if ((strlen(directoryEntry->d_name) < 5) ||
						(memcmp(".init", directoryEntry->d_name + strlen(directoryEntry->d_name) - 5, 5) &&
						 memcmp(".lock", directoryEntry->d_name + strlen(directoryEntry->d_name) - 5, 5)))
				{
					// Determine system name of port
					char* systemName = (char*)malloc(256);
					strcpy(systemName, "/dev/");
					strcat(systemName, directoryEntry->d_name);

					// Check if port is already enumerated
					serialPort *port = fetchPort(comPorts, systemName);
					if (port)
						port->enumerated = 1;
					else
					{
						// Set static friendly name
						char *location = (char*)malloc(256);
						char* friendlyName = (char*)malloc(256);
						if (directoryEntry->d_name[0] == 'c')
							strcpy(friendlyName, "Serial Port");
						else
							strcpy(friendlyName, "Serial Port (Dial-In)");

						// Add the port to the list if it is not a directory
						struct stat fileStats;
						stat(systemName, &fileStats);
						if (!S_ISDIR(fileStats.st_mode))
						{
							size_t bufferSize = 1024;
							char *stdOutResult = (char*)malloc(bufferSize), *device = NULL;
							snprintf(stdOutResult, bufferSize, "sysctl -a | grep \"ttyname: %s\"", directoryEntry->d_name + 3);
							FILE *pipe = popen(stdOutResult, "r");
							if (pipe)
							{
								while (!device && fgets(stdOutResult, bufferSize, pipe))
								{
									device = stdOutResult;
									*(strstr(device, "ttyname:") - 1) = '\0';
									strcat(device, ".%location");
								}
								pclose(pipe);
							}

							// Add port to the list and clean up memory
							if (device)
							{
								char *location = (char*)malloc(256), *temp = (char*)malloc(64);
								snprintf(location, bufferSize, "sysctl -a | grep \"%s\"", device);
								pipe = popen(location, "r");
								strcpy(location, "0-0");
								if (pipe)
								{
									while (fgets(stdOutResult, bufferSize, pipe))
										if (strstr(stdOutResult, "bus") && strstr(stdOutResult, "hubaddr") && strstr(stdOutResult, "port"))
										{
											char *cursor = strstr(stdOutResult, "bus=") + 4;
											size_t length = (size_t)(strchr(cursor, ' ') - cursor);
											memcpy(location, cursor, length);
											location[length] = '\0';
											strcat(location, "-");
											cursor = strstr(stdOutResult, "hubaddr=") + 8;
											length = (size_t)(strchr(cursor, ' ') - cursor);
											memcpy(temp, cursor, length);
											temp[length] = '\0';
											strcat(location, temp);
											strcat(location, ".");
											cursor = strstr(stdOutResult, "port=") + 5;
											length = (size_t)(strchr(cursor, ' ') - cursor);
											memcpy(temp, cursor, length);
											temp[length] = '\0';
											strcat(location, temp);
											break;
										}
									pclose(pipe);
								}
								pushBack(comPorts, systemName, friendlyName, friendlyName, location);
								free(location);
								free(temp);
							}
							else
								pushBack(comPorts, systemName, friendlyName, friendlyName, "0-0");
							free(stdOutResult);
						}

						// Clean up memory
						free(friendlyName);
						free(location);
					}

					// Clean up memory
					free(systemName);
				}
			}
			directoryEntry = readdir(directoryIterator);
		}

		// Close the directory
		closedir(directoryIterator);
	}
}

#elif defined(__APPLE__)

#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/ioss.h>
#include <IOKit/usb/USBSpec.h>
#include <sys/ioctl.h>

#if (MAC_OS_X_VERSION_MAX_ALLOWED < 120000)
  #define kIOMainPortDefault kIOMasterPortDefault
#endif

void enumeratePortsMac(serialPortVector *comPorts)
{
	serialPort *port;
	io_object_t serialPort;
	io_iterator_t serialPortIterator;
	int vendor_id = 0, product_id = 0;
	char friendlyName[1024], comPortCu[1024], comPortTty[1024];
	char portLocation[1024], portDescription[1024], serialNumber[1024] = "Unknown";

	// Enumerate serial ports on machine
	IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching(kIOSerialBSDServiceValue), &serialPortIterator);
	while ((serialPort = IOIteratorNext(serialPortIterator)))
	{
		// Get serial port information
		char isUSB = 0;
		friendlyName[0] = '\0';
		io_registry_entry_t parent = 0, service = serialPort;
		while (service)
		{
			if (IOObjectConformsTo(service, "IOUSBDevice"))
			{
				IORegistryEntryGetName(service, friendlyName);
				isUSB = 1;
				break;
			}
			if (IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent) != KERN_SUCCESS)
				break;
			if (service != serialPort)
				IOObjectRelease(service);
			service = parent;
		}
		if (service != serialPort)
			IOObjectRelease(service);

		// Get serial port name and COM value
		if (friendlyName[0] == '\0')
		{
			CFStringRef friendlyNameRef = (CFStringRef)IORegistryEntryCreateCFProperty(serialPort, CFSTR(kIOTTYDeviceKey), kCFAllocatorDefault, 0);
			CFStringGetCString(friendlyNameRef, friendlyName, sizeof(friendlyName), kCFStringEncodingUTF8);
			CFRelease(friendlyNameRef);
		}
		CFStringRef comPortRef = (CFStringRef)IORegistryEntryCreateCFProperty(serialPort, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0);
		CFStringGetCString(comPortRef, comPortCu, sizeof(comPortCu), kCFStringEncodingUTF8);
		CFRelease(comPortRef);
		comPortRef = (CFStringRef)IORegistryEntryCreateCFProperty(serialPort, CFSTR(kIODialinDeviceKey), kCFAllocatorDefault, 0);
		CFStringGetCString(comPortRef, comPortTty, sizeof(comPortTty), kCFStringEncodingUTF8);
		CFRelease(comPortRef);

		// Get VID, PID, Serial Number, Bus Number, and Port Address
		if (isUSB)
		{
			CFTypeRef propertyRef = IORegistryEntrySearchCFProperty(serialPort, kIOServicePlane, CFSTR("locationID"), kCFAllocatorDefault, kIORegistryIterateRecursively | kIORegistryIterateParents);
			if (propertyRef)
			{
				int locationID = 0;
				char multiHub = 0, tempLocation[64];
				CFNumberGetValue(propertyRef, kCFNumberIntType, &locationID);
				CFRelease(propertyRef);
				snprintf(portLocation, sizeof(portLocation), "%d", (locationID >> 24) & 0x000000FF);
				strcat(portLocation, "-");
				while (locationID & 0x00F00000)
				{
					if (multiHub)
						strcat(portLocation, ".");
					snprintf(tempLocation, sizeof(tempLocation), "%d", (locationID >> 20) & 0x0000000F);
					strcat(portLocation, tempLocation);
					locationID <<= 4;
					multiHub = 1;
				}
			}
			propertyRef = IORegistryEntrySearchCFProperty(serialPort, kIOServicePlane, CFSTR(kUSBSerialNumberString), kCFAllocatorDefault, kIORegistryIterateRecursively | kIORegistryIterateParents);
			if (propertyRef)
			{
				CFStringGetCString(propertyRef, serialNumber, sizeof(serialNumber), kCFStringEncodingASCII);
				CFRelease(propertyRef);
			}
			propertyRef = IORegistryEntrySearchCFProperty(serialPort, kIOServicePlane, CFSTR(kUSBVendorID), kCFAllocatorDefault, kIORegistryIterateRecursively | kIORegistryIterateParents);
			if (propertyRef)
			{
				CFNumberGetValue(propertyRef, kCFNumberIntType, &vendor_id);
				CFRelease(propertyRef);
			}
			propertyRef = IORegistryEntrySearchCFProperty(serialPort, kIOServicePlane, CFSTR(kUSBProductID), kCFAllocatorDefault, kIORegistryIterateRecursively | kIORegistryIterateParents);
			if (propertyRef)
			{
				CFNumberGetValue(propertyRef, kCFNumberIntType, &product_id);
				CFRelease(propertyRef);
			}
		}
		else
			strcpy(portLocation, "0-0");

		// Add ports to enumerated list
		pushBack(comPorts, comPortCu, friendlyName, friendlyName, portLocation);
		strcat(friendlyName, " (Dial-In)");
		pushBack(comPorts, comPortTty, friendlyName, friendlyName, portLocation);
		IOObjectRelease(serialPort);
	}
	IOObjectRelease(serialPortIterator);
}

#endif

int main(void)
{
	// Enumerate all serial ports
#if defined(__linux__)
	recursiveSearchForComPortsTest(&comPorts, "/sys/devices/");
	driverBasedSearchForComPortsTest(&comPorts, "/proc/tty/driver/serial", "/dev/ttyS");
	driverBasedSearchForComPortsTest(&comPorts, "/proc/tty/driver/mvebu_serial", "/dev/ttyMV");
	lastDitchSearchForComPortsTest(&comPorts);
#elif defined(__APPLE__)
	enumeratePortsMac(&comPorts);
#endif

	// Output all enumerated ports
	for (int i = 0; i < comPorts.length; ++i)
	{
		serialPort *port = comPorts.ports[i];
		printf("%s: Description = %s, Location = %s\n", port->portPath, port->friendlyName, port->portLocation);
	}

	return 0;
}
