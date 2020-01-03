/*
 * WindowsHelperFunctions.c
 *
 *       Created on:  May 05, 2015
 *  Last Updated on:  Mar 25, 2016
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

#ifdef _WIN32
#include <stdlib.h>
#include <string.h>
#include "WindowsHelperFunctions.h"

void push_back(struct charTupleVector* vector, const wchar_t* firstString, const wchar_t* secondString, const wchar_t* thirdString)
{
	// Allocate memory for new string storage
	vector->length++;
	wchar_t** newMemory = (wchar_t**)realloc(vector->first, vector->length*sizeof(wchar_t*));
	if (newMemory)
		vector->first = newMemory;
	newMemory = (wchar_t**)realloc(vector->second, vector->length*sizeof(wchar_t*));
	if (newMemory)
		vector->second = newMemory;
	newMemory = (wchar_t**)realloc(vector->third, vector->length*sizeof(wchar_t*));
	if (newMemory)
		vector->third = newMemory;

	// Store new strings
	vector->first[vector->length-1] = (wchar_t*)malloc((wcslen(firstString)+1)*sizeof(wchar_t));
	vector->second[vector->length-1] = (wchar_t*)malloc((wcslen(secondString)+1)*sizeof(wchar_t));
	vector->third[vector->length-1] = (wchar_t*)malloc((wcslen(thirdString)+1)*sizeof(wchar_t));
	wcscpy(vector->first[vector->length-1], firstString);
	wcscpy(vector->second[vector->length-1], secondString);
	wcscpy(vector->third[vector->length-1], thirdString);
}

char keyExists(struct charTupleVector* vector, const wchar_t* key)
{
	// Search for a vector item with a matching key
	size_t i;
	for (i = 0; i < vector->length; ++i)
		if (wcscmp(key, vector->first[i]) == 0)
			return 1;
	return 0;
}

#endif
