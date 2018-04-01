/*
 * WindowsHelperFunctions.c
 *
 *       Created on:  May 05, 2015
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

#ifdef _WIN32
#include <stdlib.h>
#include <string.h>
#include "WindowsHelperFunctions.h"

void push_back(struct charTupleVector* vector, const char* firstString, const char* secondString, const char* thirdString)
{
	// Allocate memory for new string storage
	vector->length++;
	char** newMemory = (char**)realloc(vector->first, vector->length*sizeof(char*));
	if (newMemory)
		vector->first = newMemory;
	newMemory = (char**)realloc(vector->second, vector->length*sizeof(char*));
	if (newMemory)
		vector->second = newMemory;
	newMemory = (char**)realloc(vector->third, vector->length*sizeof(char*));
	if (newMemory)
		vector->third = newMemory;

	// Store new strings
	vector->first[vector->length-1] = (char*)malloc(strlen(firstString)+1);
	vector->second[vector->length-1] = (char*)malloc(strlen(secondString)+1);
	vector->third[vector->length-1] = (char*)malloc(strlen(thirdString)+1);
	strcpy(vector->first[vector->length-1], firstString);
	strcpy(vector->second[vector->length-1], secondString);
	strcpy(vector->third[vector->length-1], thirdString);
}

char keyExists(struct charTupleVector* vector, const char* key)
{
	size_t i;
	for (i = 0; i < vector->length; ++i)
		if (strcmp(key, vector->first[i]) == 0)
			return 1;
	return 0;
}

#endif
