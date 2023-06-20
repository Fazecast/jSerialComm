#include "PosixHelperFunctions.h"

static serialPortVector comPorts = { NULL, 0, 0 };

int main(void)
{
	// Enumerate all serial ports
	searchForComPorts(&comPorts);

	// Output all enumerated ports
	printf("Initial enumeration:\n\n");
	for (int i = 0; i < comPorts.length; ++i)
	{
		serialPort *port = comPorts.ports[i];
		printf("\t%s: Friendly Name = %s, Description = %s, Location = %s, VID/PID = %04X/%04X, Serial = %s\n", port->portPath, port->friendlyName, port->portDescription, port->portLocation, port->vendorID, port->productID, port->serialNumber);
	}

	// Reset the enumerated flag on all non-open serial ports
	for (int i = 0; i < comPorts.length; ++i)
		comPorts.ports[i]->enumerated = (comPorts.ports[i]->handle > 0);

	// Re-enumerate all serial ports
	searchForComPorts(&comPorts);

	// Remove all non-enumerated ports from the serial port listing
	for (int i = 0; i < comPorts.length; ++i)
		if (!comPorts.ports[i]->enumerated)
		{
			removePort(&comPorts, comPorts.ports[i]);
			i--;
		}

	// Output all enumerated ports once again
	printf("\nSecond enumeration:\n\n");
	for (int i = 0; i < comPorts.length; ++i)
	{
		serialPort *port = comPorts.ports[i];
		printf("\t%s: Friendly Name = %s, Description = %s, Location = %s, VID/PID = %04X/%04X, Serial = %s\n", port->portPath, port->friendlyName, port->portDescription, port->portLocation, port->vendorID, port->productID, port->serialNumber);
	}

	// Clean up all memory and return
	cleanUpVector(&comPorts);
	return 0;
}
