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

// Runtime-loadable DLL functions
typedef int (__stdcall *FT_CreateDeviceInfoListFunction)(LPDWORD);
typedef int (__stdcall *FT_GetDeviceInfoListFunction)(FT_DEVICE_LIST_INFO_NODE*, LPDWORD);

// Global list of available serial ports
char portsEnumerated = 0;
serialPortVector serialPorts = { NULL, 0, 0 };

// Generalized port enumeration function
static void enumeratePorts(void)
{
	// Reset the enumerated flag on all non-open serial ports
	for (int i = 0; i < serialPorts.length; ++i)
		serialPorts.ports[i]->enumerated = (serialPorts.ports[i]->handle != INVALID_HANDLE_VALUE);

	// Enumerate all serial ports present on the current system
	wchar_t *deviceID = NULL;
	DWORD deviceIdLength = 0;
	const struct { GUID guid; DWORD flags; } setupClasses[] = {
			{ .guid = GUID_DEVCLASS_PORTS, .flags = DIGCF_PRESENT },
			{ .guid = GUID_DEVCLASS_MODEM, .flags = DIGCF_PRESENT },
			{ .guid = GUID_DEVCLASS_MULTIPORTSERIAL, .flags = DIGCF_PRESENT },
			{ .guid = GUID_DEVINTERFACE_COMPORT, .flags = DIGCF_PRESENT | DIGCF_DEVICEINTERFACE },
			{ .guid = GUID_DEVINTERFACE_MODEM, .flags = DIGCF_PRESENT | DIGCF_DEVICEINTERFACE }
	};
	for (int i = 0; i < (sizeof(setupClasses) / sizeof(setupClasses[0])); ++i)
	{
		HDEVINFO devList = SetupDiGetClassDevsW(&setupClasses[i].guid, NULL, NULL, setupClasses[i].flags);
		if (devList != INVALID_HANDLE_VALUE)
		{
			// Iterate through all devices
			DWORD devInterfaceIndex = 0;
			DEVPROPTYPE devInfoPropType;
			SP_DEVINFO_DATA devInfoData;
			devInfoData.cbSize = sizeof(devInfoData);
			while (SetupDiEnumDeviceInfo(devList, devInterfaceIndex++, &devInfoData))
			{
				// Attempt to determine the device's Vendor ID and Product ID
				DWORD deviceIdRequiredLength;
				int vendorID = -1, productID = -1;
				if (!SetupDiGetDeviceInstanceIdW(devList, &devInfoData, NULL, 0, &deviceIdRequiredLength) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER) && (deviceIdRequiredLength > deviceIdLength))
				{
					wchar_t *newMemory = (wchar_t*)realloc(deviceID, deviceIdRequiredLength * sizeof(wchar_t));
					if (newMemory)
					{
						deviceID = newMemory;
						deviceIdLength = deviceIdRequiredLength;
					}
				}
				if (SetupDiGetDeviceInstanceIdW(devList, &devInfoData, deviceID, deviceIdLength, NULL))
				{
					wchar_t *vendorIdString = wcsstr(deviceID, L"VID_"), *productIdString = wcsstr(deviceID, L"PID_");
					if (vendorIdString && productIdString)
					{
						*wcschr(vendorIdString, L'&') = L'\0';
						vendorID = _wtoi(vendorIdString + 4);
						productID = _wtoi(productIdString + 4);
					}
				}

				// Fetch the corresponding COM port for this device
				DWORD comPortLength = 0;
				wchar_t *comPort = NULL, *comPortString = NULL;
				char friendlyNameMemory = 0, portDescriptionMemory = 0;
				HKEY key = SetupDiOpenDevRegKey(devList, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
				if (key != INVALID_HANDLE_VALUE)
				{
					if ((RegQueryValueExW(key, L"PortName", NULL, NULL, NULL, &comPortLength) == ERROR_SUCCESS) && (comPortLength < 32))
					{
						comPortLength += sizeof(wchar_t);
						comPort = (wchar_t*)malloc(comPortLength);
						if (comPort && (RegGetValueW(key, NULL, L"PortName", RRF_RT_REG_SZ, NULL, (LPBYTE)comPort, &comPortLength) == ERROR_SUCCESS))
							comPortString = (comPort[0] == L'\\') ? (wcsrchr(comPort, L'\\') + 1) : comPort;
					}
					RegCloseKey(key);
				}
				if (!comPortString || wcsstr(comPortString, L"LPT"))
				{
					if (comPort)
						free(comPort);
					continue;
				}

				// Fetch the friendly name for this device
				DWORD friendlyNameLength = 0;
				wchar_t *friendlyNameString = NULL;
				if ((!SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_FriendlyName, &devInfoPropType, NULL, 0, &friendlyNameLength, 0) && (GetLastError() != ERROR_INSUFFICIENT_BUFFER)) || !friendlyNameLength)
					SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_FRIENDLYNAME, NULL, NULL, 0, &friendlyNameLength);
				if (friendlyNameLength && (friendlyNameLength < 256))
				{
					friendlyNameLength += sizeof(wchar_t);
					friendlyNameString = (wchar_t*)malloc(friendlyNameLength);
					if (!friendlyNameString || (SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_FriendlyName, &devInfoPropType, (BYTE*)friendlyNameString, friendlyNameLength, NULL, 0) &&
							!SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_FRIENDLYNAME, NULL, (BYTE*)friendlyNameString, friendlyNameLength, NULL)))
					{
						if (friendlyNameString)
							free(friendlyNameString);
						friendlyNameString = comPortString;
						friendlyNameLength = comPortLength;
					}
					else
					{
						friendlyNameMemory = 1;
						friendlyNameString[(friendlyNameLength / sizeof(wchar_t)) - 1] = 0;
					}
				}
				else
				{
					friendlyNameString = comPortString;
					friendlyNameLength = comPortLength;
				}

				// Fetch the bus-reported device description
				DWORD portDescriptionLength = 0;
				wchar_t *portDescriptionString = NULL;
				if ((SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &devInfoPropType, NULL, 0, &portDescriptionLength, 0) || (GetLastError() == ERROR_INSUFFICIENT_BUFFER)) && portDescriptionLength && (portDescriptionLength < 256))
				{
					portDescriptionLength += sizeof(wchar_t);
					portDescriptionString = (wchar_t*)malloc(portDescriptionLength);
					if (!portDescriptionString || !SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &devInfoPropType, (BYTE*)portDescriptionString, portDescriptionLength, NULL, 0))
					{
						if (portDescriptionString)
							free(portDescriptionString);
						portDescriptionString = friendlyNameString;
						portDescriptionLength = friendlyNameLength;
					}
					else
					{
						portDescriptionMemory = 1;
						portDescriptionString[(portDescriptionLength / sizeof(wchar_t)) - 1] = 0;
					}
				}
				else
				{
					portDescriptionString = friendlyNameString;
					portDescriptionLength = friendlyNameLength;
				}

				// Fetch the physical location for this device
				wchar_t *locationString = NULL;
				DWORD locationLength = 0, busNumber = -1, hubNumber = -1, portNumber = -1;
				if (!SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusNumber, &devInfoPropType, (BYTE*)&busNumber, sizeof(busNumber), NULL, 0) &&
						!SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_BUSNUMBER, NULL, (BYTE*)&busNumber, sizeof(busNumber), NULL))
					busNumber = -1;
				if (!SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_Address, &devInfoPropType, (BYTE*)&portNumber, sizeof(portNumber), NULL, 0) &&
						!SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_ADDRESS, NULL, (BYTE*)&portNumber, sizeof(portNumber), NULL))
					portNumber = -1;
				if ((!SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_LocationInfo, &devInfoPropType, NULL, 0, &locationLength, 0) && (GetLastError() != ERROR_INSUFFICIENT_BUFFER)) || !locationLength)
					SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_LOCATION_INFORMATION, NULL, NULL, 0, &locationLength);
				if (locationLength && (locationLength < 256))
				{
					locationLength += sizeof(wchar_t);
					locationString = (wchar_t*)malloc(locationLength);
					if (locationString && (SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_LocationInfo, &devInfoPropType, (BYTE*)locationString, locationLength, NULL, 0) ||
							SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_LOCATION_INFORMATION, NULL, (BYTE*)locationString, locationLength, NULL)))
					{
						locationString[(locationLength / sizeof(wchar_t)) - 1] = 0;
						if (wcsstr(locationString, L"Hub"))
							hubNumber = _wtoi(wcschr(wcsstr(locationString, L"Hub"), L'#') + 1);
						if ((portNumber == -1) && wcsstr(locationString, L"Port"))
						{
							wchar_t *portString = wcschr(wcsstr(locationString, L"Port"), L'#') + 1;
							if (portString)
							{
								wchar_t *end = wcschr(portString, L'.');
								if (end)
									*end = L'\0';
							}
							portNumber = _wtoi(portString);
						}
					}
					if (locationString)
						free(locationString);
				}
				if (busNumber == -1)
					busNumber = 0;
				if (hubNumber == -1)
					hubNumber = 0;
				if (portNumber == -1)
					portNumber = 0;
				locationString = (wchar_t*)malloc(16*sizeof(wchar_t));
				if (locationString)
					_snwprintf_s(locationString, 16, 16, L"%d-%d.%d", busNumber, hubNumber, portNumber);
				else
				{
					free(comPort);
					if (friendlyNameMemory)
						free(friendlyNameString);
					if (portDescriptionMemory)
						free(portDescriptionString);
					continue;
				}

				// Check if port is already enumerated
				serialPort *port = fetchPort(&serialPorts, comPortString);
				if (port)
				{
					// See if device has changed locations
					port->enumerated = 1;
					int oldLength = 1 + wcslen(port->portLocation);
					int newLength = 1 + wcslen(locationString);
					if (oldLength != newLength)
					{
						wchar_t *newMemory = (wchar_t*)realloc(port->portLocation, newLength * sizeof(wchar_t));
						if (newMemory)
						{
							port->portLocation = newMemory;
							wcscpy_s(port->portLocation, newLength, locationString);
						}
						else
							wcscpy_s(port->portLocation, oldLength, locationString);
					}
					else if (wcscmp(port->portLocation, locationString))
						wcscpy_s(port->portLocation, newLength, locationString);
				}
				else
					pushBack(&serialPorts, comPortString, friendlyNameString, portDescriptionString, locationString, vendorID, productID);

				// Clean up memory and reset device info structure
				free(comPort);
				free(locationString);
				if (friendlyNameMemory)
					free(friendlyNameString);
				if (portDescriptionMemory)
					free(portDescriptionString);
				devInfoData.cbSize = sizeof(devInfoData);
			}
			SetupDiDestroyDeviceInfoList(devList);
		}
	}

	// Attempt to locate any FTDI-specified port descriptions
	HINSTANCE ftdiLibInstance = LoadLibrary(TEXT("ftd2xx.dll"));
	if (ftdiLibInstance != NULL)
	{
		FT_CreateDeviceInfoListFunction FT_CreateDeviceInfoList = (FT_CreateDeviceInfoListFunction)GetProcAddress(ftdiLibInstance, "FT_CreateDeviceInfoList");
		FT_GetDeviceInfoListFunction FT_GetDeviceInfoList = (FT_GetDeviceInfoListFunction)GetProcAddress(ftdiLibInstance, "FT_GetDeviceInfoList");
		if (FT_CreateDeviceInfoList && FT_GetDeviceInfoList)
		{
			DWORD numDevs;
			if ((FT_CreateDeviceInfoList(&numDevs) == FT_OK) && (numDevs > 0))
			{
				FT_DEVICE_LIST_INFO_NODE *devInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*numDevs);
				if (devInfo && (FT_GetDeviceInfoList(devInfo, &numDevs) == FT_OK))
				{
					for (int i = 0; i < numDevs; ++i)
					{
						// Determine if the port is currently enumerated and already open
						char isOpen = ((devInfo[i].Flags & FT_FLAGS_OPENED) || (devInfo[i].SerialNumber[0] == 0)) ? 1 : 0;
						if (!isOpen)
							for (int j = 0; j < serialPorts.length; ++j)
								if ((memcmp(serialPorts.ports[j]->serialNumber, devInfo[i].SerialNumber, sizeof(serialPorts.ports[j]->serialNumber)) == 0) && (serialPorts.ports[j]->handle != INVALID_HANDLE_VALUE))
								{
									serialPorts.ports[j]->enumerated = 1;
									isOpen = 1;
									break;
								}

						// Update the port description if not already open
						const int comPortLength = 16;
						wchar_t *comPort = (wchar_t*)malloc(comPortLength);
						devInfo[i].Description[sizeof(devInfo[i].Description)-1] = 0;
						devInfo[i].SerialNumber[sizeof(devInfo[i].SerialNumber)-1] = 0;
						if (!isOpen && comPort && getPortPathFromSerial(comPort, comPortLength, devInfo[i].SerialNumber))
						{
							// Check if actually connected and present in the port list
							for (int j = 0; j < serialPorts.length; ++j)
								if ((wcscmp(serialPorts.ports[j]->portPath + 4, comPort) == 0) && strlen(devInfo[i].Description))
								{
									// Update the port description
									serialPorts.ports[j]->enumerated = 1;
									size_t descLength = 8 + strlen(devInfo[i].Description);
									wchar_t *newMemory = (wchar_t*)realloc(serialPorts.ports[j]->portDescription, descLength*sizeof(wchar_t));
									if (newMemory)
									{
										serialPorts.ports[j]->portDescription = newMemory;
										MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, devInfo[i].Description, -1, serialPorts.ports[j]->portDescription, descLength);
									}
									memcpy(serialPorts.ports[j]->serialNumber, devInfo[i].SerialNumber, sizeof(serialPorts.ports[j]->serialNumber));
									break;
								}
						}
						if (comPort)
							free(comPort);
					}
				}
				if (devInfo)
					free(devInfo);
			}
		}
		FreeLibrary(ftdiLibInstance);
	}

	// Attempt to locate any non-registered virtual serial ports (e.g., from VSPE)
	HKEY key, paramKey;
	DWORD keyType, numValues, maxValueLength, maxComPortLength;
	if ((RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) &&
			(RegQueryInfoKeyW(key, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, &maxValueLength, &maxComPortLength, NULL, NULL) == ERROR_SUCCESS))
	{
		// Allocate memory
		++maxValueLength;
		++maxComPortLength;
		WCHAR *valueName = (WCHAR*)malloc(maxValueLength*sizeof(WCHAR));
		WCHAR *comPort = (WCHAR*)malloc(maxComPortLength*sizeof(WCHAR));

		// Iterate through all COM ports
		for (DWORD i = 0; i < numValues; ++i)
		{
			// Get serial port name and COM value
			DWORD valueLength = maxValueLength;
			DWORD comPortLength = maxComPortLength;
			memset(valueName, 0, valueLength*sizeof(WCHAR));
			memset(comPort, 0, comPortLength*sizeof(WCHAR));
			if ((RegEnumValueW(key, i, valueName, &valueLength, NULL, &keyType, (BYTE*)comPort, &comPortLength) == ERROR_SUCCESS) && (keyType == REG_SZ))
			{
				// Set port name and description
				wchar_t* comPortString = (comPort[0] == L'\\') ? (wcsrchr(comPort, L'\\') + 1) : comPort;
				wchar_t* friendlyNameString = wcsrchr(valueName, L'\\') ? (wcsrchr(valueName, L'\\') + 1) : valueName;

				// Add new SerialComm object to vector if it does not already exist
				serialPort *port = fetchPort(&serialPorts, comPortString);
				if (port)
					port->enumerated = 1;
				else
					pushBack(&serialPorts, comPortString, friendlyNameString, L"Virtual Serial Port", L"X-X.X", -1, -1);
			}
		}

		// Clean up memory
		free(valueName);
		free(comPort);
		RegCloseKey(key);
	}

	// Clean up memory
	if (deviceID)
		free(deviceID);

	// Remove all non-enumerated ports from the serial port listing
	for (int i = 0; i < serialPorts.length; ++i)
		if (!serialPorts.ports[i]->enumerated)
		{
			removePort(&serialPorts, serialPorts.ports[i]);
			i--;
		}
	portsEnumerated = 1;
}


int main(void)
{
	// Enumerate all serial ports
	enumeratePorts();

	// Output all enumerated ports
	wprintf(L"Initial enumeration:\n\n");
	for (int i = 0; i < serialPorts.length; ++i)
	{
		serialPort *port = serialPorts.ports[i];
		wprintf(L"\t%s: Friendly Name = %s, Description = %s, Location = %s, VID/PID = %d/%d\n", port->portPath, port->friendlyName, port->portDescription, port->portLocation, port->vendorID, port->productID);
	}

	// Re-enumerate all serial ports
	enumeratePorts();

	// Output all enumerated ports once again
	wprintf(L"\nSecond enumeration:\n\n");
	for (int i = 0; i < serialPorts.length; ++i)
	{
		serialPort *port = serialPorts.ports[i];
		wprintf(L"\t%s: Friendly Name = %s, Description = %s, Location = %s, VID/PID = %d/%d\n", port->portPath, port->friendlyName, port->portDescription, port->portLocation, port->vendorID, port->productID);
	}

	// Clean up all memory and return
	cleanUpVector(&serialPorts);
	return 0;
}
