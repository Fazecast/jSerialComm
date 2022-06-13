/*
 * SerialPortInvalidPortException.java
 *
 *       Created on:  Apr 15, 2019
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

/**
 * This class describes a serial port invalid port exception.
 *
 * @see java.lang.RuntimeException
 */
public final class SerialPortInvalidPortException extends RuntimeException
{
	private static final long serialVersionUID = 3420177672598538224L;

	/**
	 * Constructs a {@link SerialPortInvalidPortException} with the specified detail message and cause.
	 * <p>
	 * Note that the detail message associated with <b>cause</b> is <i>not</i> automatically incorporated into this exception's detail message.
	 *
	 * @param message message The detail message (which is saved for later retrieval by the {@link SerialPortInvalidPortException#getMessage()} method).
	 * @param cause The cause (which is saved for later retrieval by the {@link SerialPortInvalidPortException#getCause()} method). (A null value is permitted, and indicates that the cause is nonexistent or unknown.)
	 */
	public SerialPortInvalidPortException(String message, Throwable cause)
	{
		super(message, cause);
	}
}
