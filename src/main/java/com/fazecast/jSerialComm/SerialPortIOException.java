/*
 * SerialPortIOException.java
 *
 *       Created on:  Aug 08, 2018
 *  Last Updated on:  Jun 08, 2022
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2022 Fazecast, Inc.
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

package com.fazecast.jSerialComm;

import java.io.IOException;

/**
 * This class describes a serial port IO exception.
 * 
 * @see java.io.IOException
 */
public final class SerialPortIOException extends IOException
{
	private static final long serialVersionUID = 3353684802475494674L;

	/**
	 * Constructs a {@link SerialPortIOException} with the specified detail message.
	 * 
	 * @param message The detail message (which is saved for later retrieval by the {@link SerialPortIOException#getMessage()} method).
	 */
	public SerialPortIOException(String message)
	{
		super(message);
	}
}
