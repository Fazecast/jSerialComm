/*
 * SerialPortTimeoutException.java
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

import java.io.InterruptedIOException;

/**
 * This class describes a serial port timeout exception.
 * 
 * @see java.io.InterruptedIOException
 */
public final class SerialPortTimeoutException extends InterruptedIOException
{
	private static final long serialVersionUID = 3209035213903386044L;

	/**
	 * Constructs a {@link SerialPortTimeoutException} with the specified detail message.
	 * 
	 * @param message The detail message (which is saved for later retrieval by the {@link SerialPortTimeoutException#getMessage()} method).
	 */
	public SerialPortTimeoutException(String message)
	{
		super(message);
		bytesTransferred = 0;
	}
}
