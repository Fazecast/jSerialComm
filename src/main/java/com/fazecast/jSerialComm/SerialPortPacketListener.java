/*
 * SerialPortPacketListener.java
 *
 *       Created on:  Feb 25, 2015
 *  Last Updated on:  Mar 12, 2015
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2017 Fazecast, Inc.
 *
 * This file is part of jSerialComm.
 *
 * jSerialComm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * jSerialComm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with jSerialComm.  If not, see <http://www.gnu.org/licenses/>.
 */

package com.fazecast.jSerialComm;

/**
 * This interface must be implemented to enable full packet reads using event-based serial port I/O.
 * <p>
 * <i>Note</i>: Using this interface will negate any serial port read timeout settings since they make no sense in an asynchronous context.
 * 
 * @author Will Hedgecock &lt;will.hedgecock@fazecast.com&gt;
 * @version 1.4.0
 * @see com.fazecast.jSerialComm.SerialPortDataListener
 * @see java.util.EventListener
 */
public interface SerialPortPacketListener extends SerialPortDataListener
{
	/**
	 * Must be overridden to return the desired number of bytes that <b>must</b> be read before the {@link #serialEvent(SerialPortEvent)} callback is triggered.
	 * 
	 * @return The number of bytes that must be read before the {@link #serialEvent(SerialPortEvent)} callback is triggered.
	 */
	public abstract int getPacketSize();
}
