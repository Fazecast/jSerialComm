/*
 * OSXHelperFunctions.h
 *
 *       Created on:  Jul 1, 2015
 *  Last Updated on:  Mar 25, 2016
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2018 Fazecast, Inc.
 *
 * This file is part of jSerialComm.
 *
 * jSerialComm is free software: you can redistribute it and/or modify
 * it under the terms of either the Apache Software License, version 2, or
 * the GNU Lesser General Public License as published by the Free Software
 * Foundation, version 3 or above.
 *
 * jSerialComm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of both the GNU Lesser General Public
 * License and the Apache Software License along with jSerialComm. If not,
 * see <http://www.gnu.org/licenses/> and <http://www.apache.org/licenses/>.
 */

#ifndef __OSX_HELPER_FUNCTIONS_HEADER_H__
#define __OSX_HELPER_FUNCTIONS_HEADER_H__

#include <termios.h>

speed_t getBaudRateCode(speed_t baudRate);

#endif		// #ifndef __OSX_HELPER_FUNCTIONS_HEADER_H__
