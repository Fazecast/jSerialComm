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
#include "WindowsHelperFunctions.h"

static serialPortVector serialPorts = { NULL, 0, 0 };

void getPortsWindows(void)
{
	HKEY keyHandle1, keyHandle2, keyHandle3, keyHandle4, keyHandle5;
	DWORD numSubKeys1, numSubKeys2, numSubKeys3, numValues;
	DWORD maxSubKeyLength1, maxSubKeyLength2, maxSubKeyLength3;
	DWORD maxValueLength, maxComPortLength, valueLength, comPortLength, keyType;
	DWORD subKeyLength1, subKeyLength2, subKeyLength3, friendlyNameLength;

	// Reset the enumerated flag on all non-open serial ports
	for (int i = 0; i < serialPorts.length; ++i)
		serialPorts.ports[i]->enumerated = (serialPorts.ports[i]->handle != INVALID_HANDLE_VALUE);

	// Enumerate serial ports on machine
	if ((RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_QUERY_VALUE, &keyHandle1) == ERROR_SUCCESS) &&
			(RegQueryInfoKeyW(keyHandle1, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, &maxValueLength, &maxComPortLength, NULL, NULL) == ERROR_SUCCESS))
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
			valueLength = maxValueLength;
			comPortLength = maxComPortLength;
			memset(valueName, 0, valueLength*sizeof(WCHAR));
			memset(comPort, 0, comPortLength*sizeof(WCHAR));
			if ((RegEnumValueW(keyHandle1, i, valueName, &valueLength, NULL, &keyType, (BYTE*)comPort, &comPortLength) == ERROR_SUCCESS) && (keyType == REG_SZ))
			{
				// Set port name and description
				wchar_t* comPortString = (comPort[0] == L'\\') ? (wcsrchr(comPort, L'\\') + 1) : comPort;
				wchar_t* descriptionString = wcsrchr(valueName, L'\\') ? (wcsrchr(valueName, L'\\') + 1) : valueName;

				// Check if port is already enumerated
				serialPort *port = fetchPort(&serialPorts, comPortString);
				if (port)
					port->enumerated = 1;
				else
					pushBack(&serialPorts, comPortString, descriptionString, descriptionString, L"0-0");
			}
		}

		// Clean up memory
		free(valueName);
		free(comPort);
	}
	RegCloseKey(keyHandle1);

	// Enumerate all devices on machine
	if ((RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Enum", 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &keyHandle1) == ERROR_SUCCESS) &&
			(RegQueryInfoKeyW(keyHandle1, NULL, NULL, NULL, &numSubKeys1, &maxSubKeyLength1, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS))
	{
		// Allocate memory
		++maxSubKeyLength1;
		WCHAR *subKeyName1 = (WCHAR*)malloc(maxSubKeyLength1*sizeof(WCHAR));

		// Enumerate sub-keys
		for (DWORD i1 = 0; i1 < numSubKeys1; ++i1)
		{
			subKeyLength1 = maxSubKeyLength1;
			if ((RegEnumKeyExW(keyHandle1, i1, subKeyName1, &subKeyLength1, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) &&
					(RegOpenKeyExW(keyHandle1, subKeyName1, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &keyHandle2) == ERROR_SUCCESS) &&
					(RegQueryInfoKeyW(keyHandle2, NULL, NULL, NULL, &numSubKeys2, &maxSubKeyLength2, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS))
			{
				// Allocate memory
				++maxSubKeyLength2;
				WCHAR *subKeyName2 = (WCHAR*)malloc(maxSubKeyLength2*sizeof(WCHAR));

				// Enumerate sub-keys
				for (DWORD i2 = 0; i2 < numSubKeys2; ++i2)
				{
					subKeyLength2 = maxSubKeyLength2;
					if ((RegEnumKeyExW(keyHandle2, i2, subKeyName2, &subKeyLength2, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) &&
							(RegOpenKeyExW(keyHandle2, subKeyName2, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &keyHandle3) == ERROR_SUCCESS) &&
							(RegQueryInfoKeyW(keyHandle3, NULL, NULL, NULL, &numSubKeys3, &maxSubKeyLength3, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS))
					{
						// Allocate memory
						++maxSubKeyLength3;
						WCHAR *subKeyName3 = (WCHAR*)malloc(maxSubKeyLength3*sizeof(WCHAR));

						// Enumerate sub-keys
						for (DWORD i3 = 0; i3 < numSubKeys3; ++i3)
						{
							subKeyLength3 = maxSubKeyLength3;
							if ((RegEnumKeyExW(keyHandle3, i3, subKeyName3, &subKeyLength3, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) &&
									(RegOpenKeyExW(keyHandle3, subKeyName3, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &keyHandle4) == ERROR_SUCCESS) &&
									(RegQueryInfoKeyW(keyHandle4, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, NULL, &valueLength, NULL, NULL) == ERROR_SUCCESS))
							{
								// Allocate memory
								friendlyNameLength = valueLength + 1;
								WCHAR *friendlyName = (WCHAR*)malloc(friendlyNameLength*sizeof(WCHAR));
								WCHAR *locationInfo = (WCHAR*)malloc(friendlyNameLength*sizeof(WCHAR));

								if ((RegOpenKeyExW(keyHandle4, L"Device Parameters", 0, KEY_QUERY_VALUE, &keyHandle5) == ERROR_SUCCESS) &&
									(RegQueryInfoKeyW(keyHandle5, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, NULL, &valueLength, NULL, NULL) == ERROR_SUCCESS))
								{
									// Allocate memory
									comPortLength = valueLength + 1;
									WCHAR *comPort = (WCHAR*)malloc(comPortLength*sizeof(WCHAR));

									// Attempt to get COM value and friendly port name
									if ((RegQueryValueExW(keyHandle5, L"PortName", NULL, &keyType, (BYTE*)comPort, &comPortLength) == ERROR_SUCCESS) && (keyType == REG_SZ) &&
											(RegQueryValueExW(keyHandle4, L"FriendlyName", NULL, &keyType, (BYTE*)friendlyName, &friendlyNameLength) == ERROR_SUCCESS) && (keyType == REG_SZ) &&
											(RegQueryValueExW(keyHandle4, L"LocationInformation", NULL, &keyType, (BYTE*)locationInfo, &friendlyNameLength) == ERROR_SUCCESS) && (keyType == REG_SZ))
									{
										// Set port name and description
										wchar_t* comPortString = (comPort[0] == L'\\') ? (wcsrchr(comPort, L'\\') + 1) : comPort;
										wchar_t* descriptionString = friendlyName;

										// Parse the port location
										int hub = 0, port = 0, bufferLength = 128;
										wchar_t *portLocation = (wchar_t*)malloc(bufferLength*sizeof(wchar_t));
										if (wcsstr(locationInfo, L"Port_#") && wcsstr(locationInfo, L"Hub_#"))
										{
											wchar_t *hubString = wcsrchr(locationInfo, L'#') + 1;
											hub = _wtoi(hubString);
											wchar_t *portString = wcschr(locationInfo, L'#') + 1;
											if (portString)
											{
												hubString = wcschr(portString, L'.');
												if (hubString)
													*hubString = L'\0';
											}
											port = _wtoi(portString);
											_snwprintf(portLocation, comPortLength, L"1-%d.%d", hub, port);
										}
										else
											wcscpy(portLocation, L"0-0");

										// Update friendly name if COM port is actually connected and present in the port list
										for (int i = 0; i < serialPorts.length; ++i)
											if (wcscmp(serialPorts.ports[i]->portPath, comPortString) == 0)
											{
												wchar_t *newMemory = (wchar_t*)realloc(serialPorts.ports[i]->friendlyName, (wcslen(descriptionString)+1)*sizeof(wchar_t));
												if (newMemory)
												{
													serialPorts.ports[i]->friendlyName = newMemory;
													wcscpy(serialPorts.ports[i]->friendlyName, descriptionString);
												}
												newMemory = (wchar_t*)realloc(serialPorts.ports[i]->portLocation, (wcslen(portLocation)+1)*sizeof(wchar_t));
												if (newMemory)
												{
													serialPorts.ports[i]->portLocation = newMemory;
													wcscpy(serialPorts.ports[i]->portLocation, portLocation);
												}
												break;
											}

										// Clean up memory
										free(portLocation);
									}

									// Clean up memory
									free(comPort);
								}

								// Clean up memory and close registry key
								RegCloseKey(keyHandle5);
								free(locationInfo);
								free(friendlyName);
							}

							// Close registry key
							RegCloseKey(keyHandle4);
						}

						// Clean up memory and close registry key
						RegCloseKey(keyHandle3);
						free(subKeyName3);
					}
				}

				// Clean up memory and close registry key
				RegCloseKey(keyHandle2);
				free(subKeyName2);
			}
		}

		// Clean up memory and close registry key
		RegCloseKey(keyHandle1);
		free(subKeyName1);
	}

	// Attempt to locate any device-specified port descriptions
	HDEVINFO devList = SetupDiGetClassDevsW(NULL, L"USB", NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (devList != INVALID_HANDLE_VALUE)
	{
		// Iterate through all USB-connected devices
		DWORD devInterfaceIndex = 0;
		DEVPROPTYPE devInfoPropType;
		SP_DEVINFO_DATA devInfoData;
		devInfoData.cbSize = sizeof(devInfoData);
		WCHAR comPort[128];
		while (SetupDiEnumDeviceInfo(devList, devInterfaceIndex++, &devInfoData))
		{
			// Fetch the corresponding COM port for this device
			wchar_t* comPortString = NULL;
			comPortLength = sizeof(comPort) / sizeof(WCHAR);
			keyHandle5 = SetupDiOpenDevRegKey(devList, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
			if ((keyHandle5 != INVALID_HANDLE_VALUE) && (RegQueryValueExW(keyHandle5, L"PortName", NULL, &keyType, (BYTE*)comPort, &comPortLength) == ERROR_SUCCESS) && (keyType == REG_SZ))
				comPortString = (comPort[0] == L'\\') ? (wcsrchr(comPort, L'\\') + 1) : comPort;
			if (keyHandle5 != INVALID_HANDLE_VALUE)
				RegCloseKey(keyHandle5);

			// Fetch the length of the "Bus-Reported Device Description"
			if (comPortString && !SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &devInfoPropType, NULL, 0, &valueLength, 0) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER))
			{
				// Allocate memory
				++valueLength;
				WCHAR *portDescription = (WCHAR*)malloc(valueLength);

				// Retrieve the "Bus-Reported Device Description"
				if (SetupDiGetDevicePropertyW(devList, &devInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &devInfoPropType, (BYTE*)portDescription, valueLength, NULL, 0))
				{
					// Update port description if COM port is actually connected and present in the port list
					for (int i = 0; i < serialPorts.length; ++i)
						if (wcscmp(serialPorts.ports[i]->portPath, comPortString) == 0)
						{
							wchar_t *newMemory = (wchar_t*)realloc(serialPorts.ports[i]->portDescription, (wcslen(portDescription)+1)*sizeof(wchar_t));
							if (newMemory)
							{
								serialPorts.ports[i]->portDescription = newMemory;
								wcscpy(serialPorts.ports[i]->portDescription, portDescription);
							}
						}
				}

				// Clean up memory
				free(portDescription);
			}
			devInfoData.cbSize = sizeof(devInfoData);
		}
		SetupDiDestroyDeviceInfoList(devList);
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
		printf("%ls: Description = %ls, Location = %ls\n", port->portPath, port->friendlyName, port->portLocation);
	}

	return 0;
}
