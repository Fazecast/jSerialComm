/*
 * SerialPortMessageListenerWithExceptions.java
 *
 *       Created on:  Jan 03, 2020
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
 * This interface must be implemented to enable delimited message reads using event-based serial port I/O with a custom Exception callback.
 * <p>
 * <i>Note</i>: Using this interface will negate any serial port read timeout settings since they make no sense in an asynchronous context.
 *
 * @see com.fazecast.jSerialComm.SerialPortMessageListener
 * @see com.fazecast.jSerialComm.SerialPortDataListener
 * @see java.util.EventListener
 */
public interface SerialPortMessageListenerWithExceptions extends SerialPortMessageListener
{
	/**
	 * Must be overridden to handle any Java exceptions that occur asynchronously in this data listener.
	 *
	 * @param e An {@link Exception} object containing information about the exception that occurred.
	 */
	void catchException(Exception e);
}
