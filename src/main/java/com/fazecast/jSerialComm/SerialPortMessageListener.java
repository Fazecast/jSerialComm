/*
 * SerialPortMessageListener.java
 *
 *       Created on:  Mar 14, 2019
 *  Last Updated on:  Mar 15, 2019
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2019 Fazecast, Inc.
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

import java.nio.charset.Charset;

/**
 * This interface must be implemented to enable delimited string-based message reads using event-based serial port I/O.
 * <p>
 * <i>Note</i>: Using this interface will negate any serial port read timeout settings since they make no sense in an asynchronous context.
 * 
 * @author Will Hedgecock &lt;will.hedgecock@fazecast.com&gt;
 * @version 2.5.0
 * @see com.fazecast.jSerialComm.SerialPortDataListener
 * @see java.util.EventListener
 */
public interface SerialPortMessageListener extends SerialPortDataListener
{
	/**
	 * Must be overridden to return the expected message delimiter that <b>must</b> be encountered before the {@link #serialEvent(SerialPortEvent)} callback is triggered.
	 * 
	 * @return A string indicating the expected message delimiter that must be encountered before the {@link #serialEvent(SerialPortEvent)} callback is triggered.
	 */
	public abstract String getMessageDelimiter();
	
	/**
	 * Must be overridden to return the expected character encoding used by your message transfer protocol.
	 * 
	 * @return A {@link java.nio.charset.Charset} indicating the expected character encoding used by your serial messages.
	 */
	public abstract Charset getCharacterEncoding();
}
