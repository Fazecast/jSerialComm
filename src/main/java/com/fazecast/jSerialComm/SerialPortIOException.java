/*
 * SerialPortIOException.java
 *
 *       Created on:  Aug 08, 2018
 *  Last Updated on:  Aug 08, 2018
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2018-2018 Fazecast, Inc.
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
 * @author Will Hedgecock &lt;will.hedgecock@fazecast.com&gt;
 * @version 2.5.0
 * @see java.io.IOException
 */
public final class SerialPortIOException extends IOException
{
	private static final long serialVersionUID = 3353684802475494674L;

	/**
	 * Constructs a {@link SerialPortIOException} with {@code null} as its error detail message.
	 */
	public SerialPortIOException()
	{
		super();
	}

	/**
	 * Constructs a {@link SerialPortIOException} with the specified detail message.
	 * 
	 * @param message The detail message (which is saved for later retrieval by the {@link getMessage()} method).
	 */
	public SerialPortIOException(String message)
	{
		super(message);
	}

	/**
	 * Constructs a {@link SerialPortIOException} with the specified detail message and cause.
	 * <p>
	 * Note that the detail message associated with {@link cause} is <i>not</i> automatically incorporated into this exception's detail message.
	 * 
	 * @param message message The detail message (which is saved for later retrieval by the {@link getMessage()} method).
	 * @param cause The cause (which is saved for later retrieval by the {@link getCause()} method). (A null value is permitted, and indicates that the cause is nonexistent or unknown.)
	 */
	public SerialPortIOException(String message, Throwable cause)
	{
		super(message, cause);
	}

	/**
	 * Constructs a {@link SerialPortIOException} with the specified cause and a detail message of {@code (cause==null ? null : cause.toString()) }
	 * (which typically contains the class and detail message of {@code cause}). This constructor is useful for IO exceptions that are little more
	 * than wrappers for other throwables.
	 * 
	 * @param cause The cause (which is saved for later retrieval by the {@link getCause()} method). (A null value is permitted, and indicates that the cause is nonexistent or unknown.)
	 */
	public SerialPortIOException(Throwable cause)
	{
		super(cause);
	}
}
