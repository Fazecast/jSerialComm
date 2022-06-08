/*
 * SerialPortThreadFactory.java
 *
 *       Created on:  May 31, 2022
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

import java.util.concurrent.ThreadFactory;

/**
 * This class is used to create internal jSerialComm threads.
 *
 * A user can call the {@link #set(ThreadFactory)} method to override the way in which threads are created.
 *
 * @see java.util.concurrent.ThreadFactory
 */
public class SerialPortThreadFactory
{
	// Default ThreadFactory instance
	private static ThreadFactory instance = new ThreadFactory()
	{
		@Override
		public Thread newThread(Runnable r) { return new Thread(r); }
	};

	/**
	 * Returns the current {@link java.util.concurrent.ThreadFactory} instance associated with this library.
	 *
	 * @return The current {@link java.util.concurrent.ThreadFactory} instance.
	 * @see java.util.concurrent.ThreadFactory
	 */
	public static ThreadFactory get() { return instance; }

	/**
	 * Allows a user to define a custom thread factory to be used by this library for creating new threads.
	 * <p>
	 * Such a custom factory method may be used, for example, to set all new threads to run as daemons.
	 *
	 * @param threadFactory A user-defined custom thread factory instance.
	 * @see java.util.concurrent.ThreadFactory
	 */
	public static void set(ThreadFactory threadFactory)
	{
		instance = threadFactory;
	}
}
