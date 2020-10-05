/*
 * WindowsHelperFunctions.h
 *
 *       Created on:  May 05, 2015
 *  Last Updated on:  Apr 01, 2018
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2020 Fazecast, Inc.
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

#ifndef __WINDOWS_HELPER_FUNCTIONS_HEADER_H__
#define __WINDOWS_HELPER_FUNCTIONS_HEADER_H__

typedef struct charTupleVector
{
	wchar_t **first, **second, **third;
	size_t length;
} charTupleVector;
void push_back(struct charTupleVector* vector, const wchar_t* firstString, const wchar_t* secondString, const wchar_t* thirdString);
char keyExists(struct charTupleVector* vector, const wchar_t* key);

#endif		// #ifndef __WINDOWS_HELPER_FUNCTIONS_HEADER_H__
