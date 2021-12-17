#define WINVER _WIN32_WINNT_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA
#define NTDDI_VERSION NTDDI_VISTA
#define WIN32_LEAN_AND_MEAN
#include <initguid.h>
#include <windows.h>
#include <delayimp.h>
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
typedef int (__stdcall *FT_GetComPortNumberFunction)(FT_HANDLE, LPLONG);
typedef int (__stdcall *FT_SetLatencyTimerFunction)(FT_HANDLE, UCHAR);
typedef int (__stdcall *FT_OpenFunction)(int, FT_HANDLE*);
typedef int (__stdcall *FT_CloseFunction)(FT_HANDLE);

static serialPortVector serialPorts = { NULL, 0, 0 };

void getPortsWindows(void)
{
	// Reset the enumerated flag on all non-open serial ports
	for (int i = 0; i < serialPorts.length; ++i)
		serialPorts.ports[i]->enumerated = (serialPorts.ports[i]->handle != INVALID_HANDLE_VALUE);

	// Enumerate all serial ports present on the current system
	wchar_t comPort[128];
	HDEVINFO devList = SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
	if (devList != INVALID_HANDLE_VALUE)
	{
		// Iterate through all devices
		DWORD devInterfaceIndex = 0;
		DEVPROPTYPE devInfoPropType;
		SP_DEVINFO_DATA devInfoData;
		devInfoData.cbSize = sizeof(devInfoData);
		while (SetupDiEnumDeviceInfo(devList, devInterfaceIndex++, &devInfoData))
		{
			// Fetch the corresponding COM port for this device
			wchar_t *comPortString = NULL;
			DWORD comPortLength = sizeof(comPort) / sizeof(wchar_t);
			HKEY key = SetupDiOpenDevRegKey(devList, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
			if ((key != INVALID_HANDLE_VALUE) && (RegQueryValueExW(key, L"PortName", NULL, NULL, (BYTE*)comPort, &comPortLength) == ERROR_SUCCESS))
				comPortString = (comPort[0] == L'\\') ? (wcsrchr(comPort, L'\\') + 1) : comPort;
			if (key != INVALID_HANDLE_VALUE)
				RegCloseKey(key);
			if (!comPortString)
				continue;

			// Fetch the friendly name for this device
			DWORD friendlyNameLength = 0;
			wchar_t *friendlyNameString = NULL;
			SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_FriendlyName, &devInfoPropType, NULL, 0, &friendlyNameLength, 0);
			if (!friendlyNameLength)
				SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_FRIENDLYNAME, NULL, NULL, 0, &friendlyNameLength);
			if (friendlyNameLength)
			{
				friendlyNameString = (wchar_t*)malloc(friendlyNameLength);
				if (!SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_FriendlyName, &devInfoPropType, (BYTE*)friendlyNameString, friendlyNameLength, NULL, 0) &&
						!SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_FRIENDLYNAME, NULL, (BYTE*)friendlyNameString, friendlyNameLength, NULL))
				{
					friendlyNameString = (wchar_t*)realloc(friendlyNameString, comPortLength);
					wcscpy(friendlyNameString, comPortString);
				}
			}
			else
			{
				friendlyNameString = (wchar_t*)malloc(comPortLength);
				wcscpy(friendlyNameString, comPortString);
			}

			// Fetch the bus-reported device description
			DWORD portDescriptionLength = 0;
			wchar_t *portDescriptionString = NULL;
			SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &devInfoPropType, NULL, 0, &portDescriptionLength, 0);
			if (portDescriptionLength)
			{
				portDescriptionString = (wchar_t*)malloc(portDescriptionLength);
				if (!SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &devInfoPropType, (BYTE*)portDescriptionString, portDescriptionLength, NULL, 0))
				{
					portDescriptionString = (wchar_t*)realloc(portDescriptionString, friendlyNameLength);
					wcscpy(portDescriptionString, friendlyNameString);
				}
			}
			else
			{
				portDescriptionString = (wchar_t*)malloc(friendlyNameLength);
				wcscpy(portDescriptionString, friendlyNameString);
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
			SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_LocationInfo, &devInfoPropType, NULL, 0, &locationLength, 0);
			if (!locationLength)
				SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_LOCATION_INFORMATION, NULL, NULL, 0, &locationLength);
			if (locationLength)
			{
				locationString = (wchar_t*)malloc(locationLength);
				if (SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_LocationInfo, &devInfoPropType, (BYTE*)locationString, locationLength, NULL, 0) ||
						SetupDiGetDeviceRegistryPropertyW(devList, &devInfoData, SPDRP_LOCATION_INFORMATION, NULL, (BYTE*)locationString, locationLength, NULL))
				{
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
				free(locationString);
			}
			if (busNumber == -1)
				busNumber = 0;
			if (hubNumber == -1)
				hubNumber = 0;
			if (portNumber == -1)
				portNumber = 0;
			locationString = (wchar_t*)malloc(32*sizeof(wchar_t));
			_snwprintf(locationString, 32, L"%d-%d.%d", busNumber, hubNumber, portNumber);

			// Check if port is already enumerated
			serialPort *port = fetchPort(&serialPorts, comPortString);
			if (port)
			{
				// See if device has changed locations
				port->enumerated = 1;
				int oldLength = wcslen(port->portLocation);
				int newLength = wcslen(locationString);
				if (oldLength != newLength)
				{
					port->portLocation = (wchar_t*)realloc(port->portLocation, (1 + newLength) * sizeof(wchar_t));
					wcscpy(port->portLocation, locationString);
				}
				else if (wcscmp(port->portLocation, locationString))
					wcscpy(port->portLocation, locationString);
			}
			else
				pushBack(&serialPorts, comPortString, friendlyNameString, portDescriptionString, locationString);

			// Clean up memory and reset device info structure
			free(locationString);
			free(portDescriptionString);
			free(friendlyNameString);
			devInfoData.cbSize = sizeof(devInfoData);
		}
		SetupDiDestroyDeviceInfoList(devList);
	}

	// Attempt to locate any FTDI-specified port descriptions
	HINSTANCE ftdiLibInstance = LoadLibrary(TEXT("ftd2xx.dll"));
	if (ftdiLibInstance != NULL)
	{
		FT_CreateDeviceInfoListFunction FT_CreateDeviceInfoList = (FT_CreateDeviceInfoListFunction)GetProcAddress(ftdiLibInstance, "FT_CreateDeviceInfoList");
		FT_GetDeviceInfoListFunction FT_GetDeviceInfoList = (FT_GetDeviceInfoListFunction)GetProcAddress(ftdiLibInstance, "FT_GetDeviceInfoList");
		FT_GetComPortNumberFunction FT_GetComPortNumber = (FT_GetComPortNumberFunction)GetProcAddress(ftdiLibInstance, "FT_GetComPortNumber");
		FT_OpenFunction FT_Open = (FT_OpenFunction)GetProcAddress(ftdiLibInstance, "FT_Open");
		FT_CloseFunction FT_Close = (FT_CloseFunction)GetProcAddress(ftdiLibInstance, "FT_Close");
		FT_SetLatencyTimerFunction FT_SetLatencyTimer = (FT_SetLatencyTimerFunction)GetProcAddress(ftdiLibInstance, "FT_SetLatencyTimer");
		if (FT_CreateDeviceInfoList && FT_GetDeviceInfoList && FT_GetComPortNumber && FT_Open && FT_Close && FT_SetLatencyTimer)
		{
			DWORD numDevs;
			if ((FT_CreateDeviceInfoList(&numDevs) == FT_OK) && (numDevs > 0))
			{
				FT_DEVICE_LIST_INFO_NODE *devInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*numDevs);
				if (FT_GetDeviceInfoList(devInfo, &numDevs) == FT_OK)
				{
					for (int i = 0; i < numDevs; ++i)
					{
						// Determine if the port is currently enumerated and already open
						char isOpen = (devInfo[i].Flags & FT_FLAGS_OPENED) ? 1 : 0;
						if (!isOpen)
							for (int j = 0; j < serialPorts.length; ++j)
								if ((memcmp(serialPorts.ports[j]->serialNumber, devInfo[i].SerialNumber, sizeof(serialPorts.ports[j]->serialNumber)) == 0) && (serialPorts.ports[j]->handle != INVALID_HANDLE_VALUE))
								{
									serialPorts.ports[j]->enumerated = 1;
									isOpen = 1;
									break;
								}

						// Update the port description and latency if not already open
						if (!isOpen)
						{
							LONG comPortNumber = 0;
							if ((FT_Open(i, &devInfo[i].ftHandle) == FT_OK) && (FT_GetComPortNumber(devInfo[i].ftHandle, &comPortNumber) == FT_OK))
							{
								// Reduce latency timer to minimum value of 2
								FT_SetLatencyTimer(devInfo[i].ftHandle, 2);

								// Update port description if COM port is actually connected and present in the port list
								FT_Close(devInfo[i].ftHandle);
								swprintf(comPort, sizeof(comPort) / sizeof(wchar_t), L"COM%ld", comPortNumber);
								for (int j = 0; j < serialPorts.length; ++j)
									if (wcscmp(serialPorts.ports[j]->portPath, comPort) == 0)
									{
										serialPorts.ports[j]->enumerated = 1;
										size_t descLength = 8+strlen(devInfo[i].Description);
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
						}
					}
				}
				free(devInfo);
			}
		}
		FreeLibrary(ftdiLibInstance);
	}

	// Remove all non-enumerated ports from the serial port listing
	for (int i = 0; i < serialPorts.length; ++i)
		if (!serialPorts.ports[i]->enumerated)
		{
			removePort(&serialPorts, serialPorts.ports[i]);
			i--;
		}
}

int main(void)
{
	// Enumerate all serial ports
	getPortsWindows();

	// Output all enumerated ports
	for (int i = 0; i < serialPorts.length; ++i)
	{
		serialPort *port = serialPorts.ports[i];
		printf("%ls: Friendly Name = %ls, Bus Description = %ls, Location = %ls\n", port->portPath, port->friendlyName, port->portDescription, port->portLocation);
	}

	return 0;
}
