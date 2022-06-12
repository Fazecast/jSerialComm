/*
 * SerialPortDataListener.java
 *
 *       Created on:  Feb 25, 2015
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

import java.util.EventListener;

/**
 * This interface must be implemented to enable simple event-based serial port I/O.
 * 
 * @see java.util.EventListener
 */
public interface SerialPortDataListener extends EventListener
{
	/**
	 * Must be overridden to return one or more desired event constants for which the {@link #serialEvent(SerialPortEvent)} callback should be triggered.
	 * <p>
	 * Valid event constants are:
	 * <p>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{@link SerialPort#LISTENING_EVENT_DATA_AVAILABLE}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{@link SerialPort#LISTENING_EVENT_DATA_RECEIVED}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{@link SerialPort#LISTENING_EVENT_DATA_WRITTEN}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{@link SerialPort#LISTENING_EVENT_PORT_DISCONNECTED}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{@link SerialPort#LISTENING_EVENT_BREAK_INTERRUPT}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{@link SerialPort#LISTENING_EVENT_CARRIER_DETECT}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{@link SerialPort#LISTENING_EVENT_CTS}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{@link SerialPort#LISTENING_EVENT_DSR}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{@link SerialPort#LISTENING_EVENT_RING_INDICATOR}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{@link SerialPort#LISTENING_EVENT_FRAMING_ERROR}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{@link SerialPort#LISTENING_EVENT_FIRMWARE_OVERRUN_ERROR}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{@link SerialPort#LISTENING_EVENT_SOFTWARE_OVERRUN_ERROR}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{@link SerialPort#LISTENING_EVENT_PARITY_ERROR}<br>
	 * <p>
	 * Two or more events may be OR'd together to listen for multiple events; however, if {@link SerialPort#LISTENING_EVENT_DATA_AVAILABLE} is OR'd with {@link SerialPort#LISTENING_EVENT_DATA_RECEIVED}, the {@link SerialPort#LISTENING_EVENT_DATA_RECEIVED} flag will take precedence.
	 * <p>
	 * Note that event-based <i>write</i> callbacks are only supported on Windows operating systems. As such, the {@link SerialPort#LISTENING_EVENT_DATA_WRITTEN}
	 * event will never be called on a non-Windows system.
	 * <p>
	 * It is recommended to <b>only</b> use the {@link SerialPort#LISTENING_EVENT_DATA_AVAILABLE}, {@link SerialPort#LISTENING_EVENT_DATA_RECEIVED},
	 * {@link SerialPort#LISTENING_EVENT_DATA_WRITTEN}, and/or {@link SerialPort#LISTENING_EVENT_PORT_DISCONNECTED} listening events in production or cross-platform code
	 * since underlying differences and lack of support for the control line status and error events among the various operating systems and device drivers make it
	 * unlikely that code listening for these events will behave similarly across different serial devices or OS's, if it works at all.
	 * 
	 * @return The event constants that should trigger the {@link #serialEvent(SerialPortEvent)} callback.
	 * @see SerialPort#LISTENING_EVENT_DATA_AVAILABLE
	 * @see SerialPort#LISTENING_EVENT_DATA_RECEIVED
	 * @see SerialPort#LISTENING_EVENT_DATA_WRITTEN
	 * @see SerialPort#LISTENING_EVENT_PORT_DISCONNECTED
	 * @see SerialPort#LISTENING_EVENT_BREAK_INTERRUPT
	 * @see SerialPort#LISTENING_EVENT_CARRIER_DETECT
	 * @see SerialPort#LISTENING_EVENT_CTS
	 * @see SerialPort#LISTENING_EVENT_DSR
	 * @see SerialPort#LISTENING_EVENT_RING_INDICATOR
	 * @see SerialPort#LISTENING_EVENT_FRAMING_ERROR
	 * @see SerialPort#LISTENING_EVENT_FIRMWARE_OVERRUN_ERROR
	 * @see SerialPort#LISTENING_EVENT_SOFTWARE_OVERRUN_ERROR
	 * @see SerialPort#LISTENING_EVENT_PARITY_ERROR
	 */
	int getListeningEvents();
	
	/**
	 * Called whenever one or more of the serial port events specified by the {@link #getListeningEvents()} method occurs.
	 * <p>
	 * Note that your implementation of this function should always perform as little data processing as possible, as the speed at which this callback will fire is at the mercy of the underlying operating system. If you need to collect a large amount of data, application-level buffering should be implemented and data processing should occur on a separate thread.
	 * 
	 * @param event A {@link SerialPortEvent} object containing information and/or data about the serial events that occurred.
	 * @see SerialPortEvent
	 */
	void serialEvent(SerialPortEvent event);
}
