#!/usr/bin/env python3

import sys
import urllib.request

if __name__ == '__main__':

	if (len(sys.argv) != 2) or ((sys.argv[1] != '-a') and (sys.argv[1] != '-n')):
		print('\nUSAGE: ./scrapeFtdiDeviceIDs FLAG\n\nFLAG Options:\n\n\t-a   Return all valid FTDI IDs\n\t-n   Only return non-standard FTDI IDs\n')
		sys.exit(0)
	omitted_vid = '0x0403' if sys.argv[1] == '-n' else ''

	ftdi_id_definitions = {}
	ftdi_ids_file = urllib.request.urlopen('https://raw.githubusercontent.com/torvalds/linux/master/drivers/usb/serial/ftdi_sio_ids.h')
	for line in ftdi_ids_file:
		decoded_line = line.decode('utf-8')
		if decoded_line.find('#define') != -1:
			tokens = decoded_line.split()
			ftdi_id_definitions[tokens[1]] = tokens[2].lower()

	ftdi_ids = []
	ftdi_ids_file = urllib.request.urlopen('https://raw.githubusercontent.com/torvalds/linux/master/drivers/usb/serial/ftdi_sio.c')
	for line in ftdi_ids_file:
		decoded_line = line.decode('utf-8')
		if decoded_line.find('USB_DEVICE') != -1:
			tokens = decoded_line.split('(')[1].split(')')[0].split(',')
			if ftdi_id_definitions[tokens[0].strip()] != omitted_vid:
				ftdi_ids.append([ftdi_id_definitions[tokens[0].strip()], ftdi_id_definitions[tokens[1].strip()]])

	result_string = ''
	for idx, ftdi_id in enumerate(ftdi_ids):
		result_string += 'makeVidPid({}, {}),{}'.format(ftdi_id[0], ftdi_id[1], '\n' if ((idx + 1) % 5) == 0 else ' ')
	print(result_string)
