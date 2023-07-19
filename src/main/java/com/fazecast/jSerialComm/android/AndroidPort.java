/*
 * AndroidPort.java
 *
 *       Created on:  Feb 15, 2022
 *  Last Updated on:  Jul 23, 2023
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2022-2023 Fazecast, Inc.
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

package com.fazecast.jSerialComm.android;

import com.fazecast.jSerialComm.SerialPort;

import android.app.Application;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.util.Log;

import java.util.HashMap;

public abstract class AndroidPort
{
	// USB request and recipient types
	protected static final int USB_REQUEST_TYPE_STANDARD = 0;
	protected static final int USB_REQUEST_TYPE_CLASS = 0x01 << 5;
	protected static final int USB_REQUEST_TYPE_VENDOR = 0x02 << 5;
	protected static final int USB_REQUEST_TYPE_RESERVED = 0x03 << 5;
	protected static final int USB_ENDPOINT_IN = 0x80;
	protected static final int USB_ENDPOINT_OUT = 0x00;
	protected static final int USB_RECIPIENT_DEVICE = 0x00;
	protected static final int USB_RECIPIENT_INTERFACE = 0x01;
	protected static final int USB_RECIPIENT_ENDPOINT = 0x02;
	protected static final int USB_RECIPIENT_OTHER = 0x03;

	// Static shared port parameters
	protected static Application context = null;
	protected static UsbManager usbManager = null;

	// Static private port parameters
	private static final String ACTION_USB_PERMISSION = "com.fazecast.jSerialComm.USB_PERMISSION";
	private static volatile boolean userPermissionGranted = false, awaitingUserPermission = false;
	private static PendingIntent permissionIntent = null;

	// Shared serial port parameters
	protected final UsbDevice usbDevice;
	protected UsbInterface usbInterface = null;
	protected UsbDeviceConnection usbConnection = null;
	protected UsbEndpoint usbDeviceIn = null, usbDeviceOut = null;
	protected volatile int writeBufferIndex = 0, writeBufferLength = 1024;
	protected volatile int readBufferIndex = 0, readBufferOffset = 0, readBufferLength = 1024;
	protected final byte[] readBuffer = new byte[readBufferLength], writeBuffer = new byte[writeBufferLength];

	// Private constructor so that class can only be created by the enumeration method
	protected AndroidPort(UsbDevice device) { usbDevice = device; }

	// Method to set the Android application context
	public static void setAndroidContext(Application androidContext) { context = androidContext; }

	// USB event handler
	private static final BroadcastReceiver usbReceiver = new BroadcastReceiver() {
		public void onReceive(Context context, Intent intent) {
			if (intent.getAction().equals(ACTION_USB_PERMISSION)) {
				synchronized (AndroidPort.class) {
					userPermissionGranted = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false);
					awaitingUserPermission = false;
					AndroidPort.class.notifyAll();
				}
			} else if (intent.getAction().equals(UsbManager.ACTION_USB_DEVICE_DETACHED)) {
				UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
				if (device != null)
					Log.i("jSerialComm", "Port was disconnected. Need TODO something");
				// TODO: Alert event waiting thread, close port, set to null
			}
		}
	};

	// Port enumeration method
	public static SerialPort[] getCommPortsNative()
	{
		// Ensure that the Android application context has been specified 
		if (context == null)
			throw new RuntimeException("The Android application context must be specified using 'setAndroidContext()' before making any jSerialComm library calls.");

		// Ensure that the device has a USB Manager
		if (!context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_USB_HOST))
			return new SerialPort[0];

		// Register a listener to handle permission request responses
		if (permissionIntent == null) {
			permissionIntent = PendingIntent.getBroadcast(context, 0, new Intent(ACTION_USB_PERMISSION), 0);
			IntentFilter filter = new IntentFilter(ACTION_USB_PERMISSION);
			filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
			context.registerReceiver(usbReceiver, filter);
		}

		// Enumerate all serial ports on the device
		usbManager = (UsbManager)context.getApplicationContext().getSystemService(Context.USB_SERVICE);
		HashMap<String, UsbDevice> deviceList = usbManager.getDeviceList();
		SerialPort[] portsList = new SerialPort[deviceList.size()];

		// Create and return the SerialPort port listing
		int i = 0;
		for (UsbDevice device : deviceList.values()) {
			// TODO: Determine the type of port
			AndroidPort androidPort = null;

			// Create a new serial port object and add it to the port listing
			SerialPort serialPort = new SerialPort(androidPort, "COM" + (i+1), device.getDeviceName(), device.getProductName(), device.getSerialNumber(), device.getSerialNumber(), device.getVendorId(), device.getProductId());
			portsList[i++] = serialPort;

			Log.i("jSerialComm", "System Port Name: " + serialPort.getSystemPortName());
			Log.i("jSerialComm", "System Port Path: " + serialPort.getSystemPortPath());
			Log.i("jSerialComm", "Descriptive Port Name: " + serialPort.getDescriptivePortName());
			Log.i("jSerialComm", "Port Description: " + serialPort.getPortDescription());
			Log.i("jSerialComm", "Serial Number: " + serialPort.getSerialNumber());
			Log.i("jSerialComm", "Location: " + serialPort.getPortLocation());
			Log.i("jSerialComm", "Vendor ID: " + serialPort.getVendorID());
			Log.i("jSerialComm", "Product ID: " + serialPort.getProductID());
		}
		return portsList;
	}

	// Native port opening method
	public long openPortNative(SerialPort serialPort)
	{
		// Obtain user permission to open the port
		if (!usbManager.hasPermission(usbDevice)) {
			synchronized (AndroidPort.class) {
				awaitingUserPermission = true;
				while (awaitingUserPermission) {
					usbManager.requestPermission(usbDevice, permissionIntent);
					try { AndroidPort.class.wait(); } catch (InterruptedException ignored) { }
				}
				if (!userPermissionGranted)
					return 0L;
			}
		}

		// Open and configure the port using chip-specific methods
		usbConnection = usbManager.openDevice(usbDevice);
		if ((usbConnection == null) || !openPort() || !configPort(serialPort))
			closePortNative();

		// Return whether the port was successfully opened
		return (usbConnection != null) ? 1L: 0L;
	}

	// Native port closing method
	public long closePortNative()
	{
		// Close the port using chip-specific methods
		if ((usbConnection != null) && closePort()) {
			usbConnection.close();
			usbConnection = null;
			usbInterface = null;
			usbDeviceOut = null;
			usbDeviceIn = null;
		}

		// Return whether the port was successfully closed
		return (usbConnection == null) ? 0L : 1L;
	}

	// Shared VID/PID-to-long creation method
	protected static long makeVidPid(int vid, int pid) { return (((long)vid << 16) & 0xFFFF0000) | ((long)pid & 0x0000FFFF); }

	// Android Port required interface
	public abstract boolean openPort();
	public abstract boolean closePort();
	public abstract boolean configPort(SerialPort serialPort);
	public abstract boolean flushRxTxBuffers();
	public abstract int waitForEvent();
	public abstract int bytesAvailable();
	public abstract int bytesAwaitingWrite();
	public abstract int readBytes(byte[] buffer, long bytesToRead, long offset, int timeoutMode, int readTimeout);
	public abstract int writeBytes(byte[] buffer, long bytesToWrite, long offset, int timeoutMode);
	public abstract void setEventListeningStatus(boolean eventListenerRunning);
	public abstract boolean setBreak();
	public abstract boolean clearBreak();
	public abstract boolean setRTS();
	public abstract boolean clearRTS();
	public abstract boolean setDTR();
	public abstract boolean clearDTR();
	public abstract boolean getCTS();
	public abstract boolean getDSR();
	public abstract boolean getDCD();
	public abstract boolean getDTR();
	public abstract boolean getRTS();
	public abstract boolean getRI();
	public abstract int getLastErrorLocation();
	public abstract int getLastErrorCode();
}
