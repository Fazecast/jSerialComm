/*
 * AndroidHelperFunctions.cpp
 *
 *       Created on:  Mar 10, 2015
 *  Last Updated on:  Mar 10, 2015
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2015 Fazecast, Inc.
 *
 * This file is part of jSerialComm.
 *
 * jSerialComm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * jSerialComm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with jSerialComm.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef __linux__
#include <fstream>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include "AndroidHelperFunctions.h"

std::string getFriendlyName(const std::string& fullPathToSearch)
{
	std::string friendlyName;

	std::ifstream input((fullPathToSearch + std::string("product")).c_str(), std::ios::in | std::ios::binary);
	if (input.is_open())
	{
		std::getline(input, friendlyName);
		input.close();
	}

	return friendlyName;
}

std::vector< std::pair<std::string, std::string> > recursiveSearchForComPorts(const std::string& fullPathToSearch)
{
	// Open the directory
	std::vector< std::pair<std::string, std::string> > comPorts;
	DIR *directoryIterator = opendir(fullPathToSearch.c_str());
	if (directoryIterator == NULL)
		return comPorts;

	// Read all sub-directories in the current directory
	struct dirent *directoryEntry = readdir(directoryIterator);
	while (directoryEntry != NULL)
	{
		// Check if entry is a sub-directory
		if (directoryEntry->d_type == DT_DIR)
		{
			// Only process non-dot, non-virtual directories
			if ((directoryEntry->d_name[0] != '.') && (strcmp(directoryEntry->d_name, "virtual") != 0))
			{
				// See if the directory names a potential serial port
				if ((strlen(directoryEntry->d_name) > 3) && (directoryEntry->d_name[0] == 't') && (directoryEntry->d_name[1] == 't') && (directoryEntry->d_name[2] == 'y'))
				{
					// See if device has a registered friendly name
					std::string friendlyName = getFriendlyName(fullPathToSearch + std::string(directoryEntry->d_name) + std::string("/device/../"));
					if (!friendlyName.empty())
						comPorts.push_back(std::make_pair(std::string("/dev/") + std::string(directoryEntry->d_name), friendlyName));
				}
				else
				{
					// Search for more serial ports within the directory
					std::vector< std::pair<std::string, std::string> > newComPorts = recursiveSearchForComPorts(fullPathToSearch + std::string(directoryEntry->d_name) + std::string("/"));
					comPorts.insert(comPorts.end(), newComPorts.begin(), newComPorts.end());
				}
			}
		}
		directoryEntry = readdir(directoryIterator);
	}

	// Close the directory
	closedir(directoryIterator);
	return comPorts;
}

#endif
