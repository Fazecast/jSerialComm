/*
 * SerialPortDataListenerWithExceptions.java
 *
 *       Created on:  Jul 11, 2019
 *  Last Updated on:  Aug 28, 2019
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

package com.fazecast.jSerialComm;

/**
 * This interface must be implemented to enable simple event-based serial port I/O with a custom Exception callback.
 *
 * @author Will Hedgecock &lt;will.hedgecock@fazecast.com&gt;
 * @version 2.7.0
 * @see com.fazecast.jSerialComm.SerialPortDataListener
 * @see java.util.EventListener
 */
public interface SerialPortDataListenerWithExceptions extends SerialPortDataListener
{
	/**
	 * Must be overridden to handle any Java exceptions that occur asynchronously in this data listener.
	 *
	 * @param e An {@link Exception} object containing information about the exception that occurred.
	 */
	public abstract void catchException(Exception e);
}
