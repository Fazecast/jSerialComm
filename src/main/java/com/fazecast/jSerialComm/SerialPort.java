/*
 * SerialPort.java
 *
 *       Created on:  Feb 25, 2012
 *  Last Updated on:  Dec 05, 2016
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

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.InputStreamReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Date;

/**
 * This class provides native access to serial ports and devices without requiring external libraries or tools.
 *
 * @author Will Hedgecock &lt;will.hedgecock@fazecast.com&gt;
 * @version 1.4.0
 * @see java.io.InputStream
 * @see java.io.OutputStream
 */
public final class SerialPort
{
	// Static initializer loads correct native library for this machine
	private static volatile boolean isAndroid = false;
	private static volatile boolean isLinuxOrMac = false;
	private static volatile boolean isWindows = false;
	static
	{
		// Determine the temporary file directory for Java
		String OS = System.getProperty("os.name").toLowerCase();
		String libraryPath = "", fileName = "";
		String tempFileDirectory = System.getProperty("java.io.tmpdir");
		if ((tempFileDirectory.charAt(tempFileDirectory.length()-1) != '\\') &&
				(tempFileDirectory.charAt(tempFileDirectory.length()-1) != '/'))
			tempFileDirectory += "/";

		// Force remove any previous versions of this library
		File directory = new File(tempFileDirectory);
		if (directory.exists())
		{
			File directoryListing[] = directory.listFiles();
			for (File listing : directoryListing)
				if (listing.isFile() && listing.toString().contains("jSerialComm"))
					listing.delete();
		}

		// Determine Operating System and architecture
		if (System.getProperty("java.vm.vendor").toLowerCase().contains("android"))
		{
			try
			{
				BufferedReader buildPropertiesFile = new BufferedReader(new FileReader("/system/build.prop"));
				String line;
				while ((line = buildPropertiesFile.readLine()) != null)
				{
					if (!line.contains("#") &&
							(line.contains("ro.product.cpu.abi") || line.contains("ro.product.cpu.abi2") || line.contains("ro.product.cpu.abilist") ||
									line.contains("ro.product.cpu.abilist64") || line.contains("ro.product.cpu.abilist32")))
					{
						libraryPath = (line.indexOf(',') == -1) ? "Android/" + line.substring(line.indexOf('=')+1) :
							"Android/" + line.substring(line.indexOf('=')+1, line.indexOf(','));
						break;
					}
				}
				buildPropertiesFile.close();
			}
			catch (Exception e) { e.printStackTrace(); }

			if (libraryPath.isEmpty())
				libraryPath = "Android/armeabi";
			isAndroid = true;
			fileName = "libjSerialComm.so";
		}
		else if (OS.indexOf("win") >= 0)
		{
			if (System.getProperty("os.arch").indexOf("64") >= 0)
				libraryPath = "Windows/x86_64";
			else
				libraryPath = "Windows/x86";
			isWindows = true;
			fileName = "jSerialComm.dll";
		}
		else if (OS.indexOf("mac") >= 0)
		{
			if (System.getProperty("os.arch").indexOf("64") >= 0)
				libraryPath = "OSX/x86_64";
			else
				libraryPath = "OSX/x86";
			isLinuxOrMac = true;
			fileName = "libjSerialComm.jnilib";
		}
		else if ((OS.indexOf("nix") >= 0) || (OS.indexOf("nux") >= 0))
		{
			if (System.getProperty("os.arch").indexOf("arm") >= 0)
			{
				// Determine the specific ARM architecture of this device
				try
				{
					BufferedReader cpuPropertiesFile = new BufferedReader(new FileReader("/proc/cpuinfo"));
					String line;
					while ((line = cpuPropertiesFile.readLine()) != null)
					{
						if (line.contains("ARMv"))
						{
							libraryPath = "Linux/armv" + line.substring(line.indexOf("ARMv")+4, line.indexOf("ARMv")+5);
							break;
						}
					}
					cpuPropertiesFile.close();
				}
				catch (Exception e) { e.printStackTrace(); }

				// Ensure that there was no error
				if (libraryPath.isEmpty())
					libraryPath = "Linux/armv6";

				// See if we need to use the hard-float dynamic linker
				File linkerFile = new File("/lib/ld-linux-armhf.so.3");
				if (linkerFile.exists())
					libraryPath += "-hf";
			}
			else if (System.getProperty("os.arch").indexOf("64") >= 0)
				libraryPath = "Linux/x86_64";
			else
				libraryPath = "Linux/x86";
			isLinuxOrMac = true;
			fileName = "libjSerialComm.so";
		}
		else
		{
			System.err.println("This operating system is not supported by the jSerialComm library.");
			System.exit(-1);
		}

		// Get path of native library and copy file to working directory
		String tempFileName = tempFileDirectory + (new Date()).getTime() + "-" + fileName;
		File tempNativeLibrary = new File(tempFileName);
		tempNativeLibrary.deleteOnExit();
		try
		{
			InputStream fileContents = SerialPort.class.getResourceAsStream("/" + libraryPath + "/" + fileName);
			if (fileContents == null)
			{
				System.err.println("Could not locate or access the native jSerialComm shared library.");
				System.err.println("If you are using multiple projects with interdependencies, you may need to fix your build settings to ensure that library resources are copied properly.");
			}
			else
			{
				FileOutputStream destinationFileContents = new FileOutputStream(tempNativeLibrary);
				byte transferBuffer[] = new byte[4096];
				int numBytesRead;
	
				while ((numBytesRead = fileContents.read(transferBuffer)) > 0)
					destinationFileContents.write(transferBuffer, 0, numBytesRead);
	
				fileContents.close();
				destinationFileContents.close();
				
				// Load native library
				System.load(tempFileName);
				initializeLibrary();
			}
		}
		catch (Exception e) { e.printStackTrace(); }
	}
	
	// Static symbolic link testing function
	private static boolean isSymbolicLink(File file) throws IOException
	{
		File canonicalFile = (file.getParent() == null) ? file : new File(file.getParentFile().getCanonicalFile(), file.getName());
		return !canonicalFile.getCanonicalFile().equals(canonicalFile.getAbsoluteFile());
	}

	/**
	 * Returns a list of all available serial ports on this machine.
	 * <p>
	 * The serial ports can be accessed by iterating through each of the SerialPort objects in this array.
	 * <p>
	 * Note that the {@link #openPort()} method must be called before any attempts to read from or write to the port.  Likewise, {@link #closePort()} should be called when you are finished accessing the port.
	 * <p>
	 * All serial port parameters or timeouts can be changed at any time after the port has been opened.
	 *
	 * @return An array of {@link SerialPort} objects.
	 */
	static public native SerialPort[] getCommPorts();

	/**
	 * Allocates a {@link SerialPort} object corresponding to the user-specified port descriptor.
	 * <p>
	 * On Windows machines, this descriptor should be in the form of "COM[*]".<br>
	 * On Linux machines, the descriptor will look similar to "/dev/tty[*]".
	 *
	 * @param portDescriptor The desired serial port to use with this library.
	 * @return A SerialPort object.
	 */
	static public SerialPort getCommPort(String portDescriptor)
	{
		// Correct port descriptor, if needed
		try
		{
			// Resolve home directory ~
			if (portDescriptor.startsWith("~" + File.separator))
				portDescriptor = System.getProperty("user.home") + portDescriptor.substring(1);
			
			// See what kind of descriptor was passed in
			if (isWindows)
				portDescriptor = "\\\\.\\" + portDescriptor.substring(portDescriptor.lastIndexOf('\\')+1);
			else if (isSymbolicLink(new File(portDescriptor)))
				portDescriptor = (new File(portDescriptor)).getCanonicalPath();
			else if (portDescriptor.contains("/pts/"))
				portDescriptor = "/dev/pts/" + portDescriptor.substring(portDescriptor.lastIndexOf('/')+1);
			else if (!((new File(portDescriptor)).exists()))
				portDescriptor = "/dev/" + portDescriptor.substring(portDescriptor.lastIndexOf('/')+1);
		}
		catch (Exception e)
		{
			SerialPort serialPort = new SerialPort();
			serialPort.comPort = "/dev/null";
			serialPort.portString = "Bad Port";
			return serialPort;
		}

		// Create SerialPort object
		SerialPort serialPort = new SerialPort();
		serialPort.comPort = portDescriptor;
		serialPort.portString = "User-Specified Port";

		return serialPort;
	}

//	/**
//	 * Allocates a {@link SerialPort} object corresponding to a previously opened serial port file descriptor.
//	 * <p>
//	 * Using this method to create a {@link SerialPort} object may not allow you to change some port characteristics
//	 * like baud rate, flow control, or parity, depending on the methodology that was used to initially open the native
//	 * file descriptor.
//	 * <p>
//	 * Use of this constructor is not recommended <b>except</b> for use with Android-specific applications. In this
//	 * case, you can use the Android USB Host APIs to allow the user to grant permission to use the port, and then
//	 * return the native file descriptor to this library via the Android UsbDeviceConnection.getFileDescriptor() method.
//	 * <p>
//	 * For non-Android applications, any of the other constructors are recommended; however, this method may still be
//	 * used if you have a specific need for it in your application.
//	 *
//	 * @param nativeFileDescriptor A pre-opened file descriptor corresponding to the serial port you would like to use with this library.
//	 * @return A SerialPort object.
//	 */
/*	static public SerialPort getCommPort(long nativeFileDescriptor)
	{
		// Create SerialPort object and associate it with the native file descriptor
		SerialPort serialPort = new SerialPort();
		if (!serialPort.associateNativeHandle(nativeFileDescriptor))
			serialPort.comPort = "UNKNOWN";
		serialPort.portString = "User-Specified Port";
		serialPort.portHandle = nativeFileDescriptor;
		serialPort.isOpened = true;

		// Create the Input/OutputStream interfaces
		serialPort.inputStream = serialPort.new SerialPortInputStream();
		serialPort.outputStream = serialPort.new SerialPortOutputStream();

		return serialPort;
	}*/

	// Parity Values
	static final public int NO_PARITY = 0;
	static final public int ODD_PARITY = 1;
	static final public int EVEN_PARITY = 2;
	static final public int MARK_PARITY = 3;
	static final public int SPACE_PARITY = 4;

	// Number of Stop Bits
	static final public int ONE_STOP_BIT = 1;
	static final public int ONE_POINT_FIVE_STOP_BITS = 2;
	static final public int TWO_STOP_BITS = 3;

	// Flow Control constants
	static final public int FLOW_CONTROL_DISABLED = 0x00000000;
	static final public int FLOW_CONTROL_RTS_ENABLED = 0x00000001;
	static final public int FLOW_CONTROL_CTS_ENABLED = 0x00000010;
	static final public int FLOW_CONTROL_DSR_ENABLED = 0x00000100;
	static final public int FLOW_CONTROL_DTR_ENABLED = 0x00001000;
	static final public int FLOW_CONTROL_XONXOFF_IN_ENABLED = 0x00010000;
	static final public int FLOW_CONTROL_XONXOFF_OUT_ENABLED = 0x00100000;

	// Timeout Modes
	static final public int TIMEOUT_NONBLOCKING = 0x00000000;
	static final public int TIMEOUT_READ_SEMI_BLOCKING = 0x00000001;
	static final public int TIMEOUT_WRITE_SEMI_BLOCKING = 0x00000010;
	static final public int TIMEOUT_READ_BLOCKING = 0x00000100;
	static final public int TIMEOUT_WRITE_BLOCKING = 0x00001000;
	static final public int TIMEOUT_SCANNER = 0x00010000;

	// Serial Port Listening Events
	static final public int LISTENING_EVENT_DATA_AVAILABLE = 0x00000001;
	static final public int LISTENING_EVENT_DATA_RECEIVED = 0x00000010;
	static final public int LISTENING_EVENT_DATA_WRITTEN = 0x00000100;

	// Serial Port Parameters
	private volatile long portHandle = -1;
	private volatile int baudRate = 9600, dataBits = 8, stopBits = ONE_STOP_BIT, parity = NO_PARITY, eventFlags = 0;
	private volatile int timeoutMode = TIMEOUT_NONBLOCKING, readTimeout = 0, writeTimeout = 0, flowControl = 0;
	private volatile SerialPortInputStream inputStream = null;
	private volatile SerialPortOutputStream outputStream = null;
	private volatile SerialPortDataListener userDataListener = null;
	private volatile SerialPortEventListener serialEventListener = null;
	private volatile String portString, comPort;
	private volatile boolean isOpened = false;

	/**
	 * Opens this serial port for reading and writing with an optional delay time.
	 * <p>
	 * All serial port parameters or timeouts can be changed at any time before or after the port has been opened.
	 * <p>
	 * Note that calling this method on an already opened port will simply return a value of true.
	 *
	 * @param safetySleepTime The number of milliseconds to sleep before opening the port in case of frequent closing/openings.
	 * @return Whether the port was successfully opened.
	 */
	public final boolean openPort(int safetySleepTime)
	{
		// Return true if already opened
		if (isOpened)
			return true;

		// If this is an Android root application, we must explicitly allow serial port access to the library
		if (isAndroid)
		{
			Process process = null;
			try
			{
				process = Runtime.getRuntime().exec("su");
				DataOutputStream writer = new DataOutputStream(process.getOutputStream());
				writer.writeBytes("chmod 666 " + comPort + "\n");
				writer.writeBytes("exit\n");
				writer.flush();
				BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
				while (reader.readLine() != null);
			}
			catch (Exception e)
			{
				e.printStackTrace();
				return false;
			}
			finally
			{
				if (process == null)
					return false;
				try { process.waitFor(); } catch (InterruptedException e) { e.printStackTrace(); return false; }
				try { process.getInputStream().close(); } catch (IOException e) { e.printStackTrace(); return false; }
				try { process.getOutputStream().close(); } catch (IOException e) { e.printStackTrace(); return false; }
				try { process.getErrorStream().close(); } catch (IOException e) { e.printStackTrace(); return false; }
				try { Thread.sleep(500); } catch (InterruptedException e) { e.printStackTrace(); return false; }
			}
		}
		
		// Force a sleep to ensure that the port does not become unusable due to rapid closing/opening on the part of the user
		if (safetySleepTime > 0)
			try { Thread.sleep(safetySleepTime); } catch (Exception e) { e.printStackTrace(); }
		
		if ((portHandle = openPortNative()) > 0)
		{
			inputStream = new SerialPortInputStream();
			outputStream = new SerialPortOutputStream();
			if (serialEventListener != null)
				serialEventListener.startListening();
		}
		return isOpened;
	}
	
	/**
	 * Opens this serial port for reading and writing.
	 * <p>
	 * This method is equivalent to calling {@link #openPort} with a value of 1000.
	 * <p>
	 * All serial port parameters or timeouts can be changed at any time before or after the port has been opened.
	 * <p>
	 * Note that calling this method on an already opened port will simply return a value of true.
	 *
	 * @return Whether the port was successfully opened.
	 */
	public final boolean openPort() { return openPort(1000); }

	/**
	 * Closes this serial port.
	 * <p>
	 * Note that calling this method on an already closed port will simply return a value of true.
	 *
	 * @return Whether the port was successfully closed.
	 */
	public final boolean closePort()
	{
		if (serialEventListener != null)
			serialEventListener.stopListening();
		if (isOpened && closePortNative(portHandle))
		{
			inputStream = null;
			outputStream = null;
			portHandle = -1;
		}
		return !isOpened;
	}

	/**
	 * Returns whether the port is currently open and available for communication.
	 *
	 * @return Whether the port is opened.
	 */
	public final boolean isOpen() { return isOpened; }

	// Serial Port Setup Methods
	private static native void initializeLibrary();						// Initializes the JNI code
	private static native void uninitializeLibrary();					// Un-initializes the JNI code
	private final native long openPortNative();							// Opens serial port
	//private final native boolean associateNativeHandle(long portHandle);// Associates an already opened file descriptor with this class
	private final native boolean closePortNative(long portHandle);		// Closes serial port
	private final native boolean configPort(long portHandle);			// Changes/sets serial port parameters as defined by this class
	private final native boolean configTimeouts(long portHandle);		// Changes/sets serial port timeouts as defined by this class
	private final native boolean configEventFlags(long portHandle);		// Changes/sets which serial events to listen for as defined by this class
	private final native int waitForEvent(long portHandle);				// Waits for serial event to occur as specified in eventFlags
	private final native int bytesAvailable(long portHandle);			// Returns number of bytes available for reading
	private final native int bytesAwaitingWrite(long portHandle);		// Returns number of bytes still waiting to be written
	private final native int readBytes(long portHandle, byte[] buffer, long bytesToRead);	// Reads bytes from serial port
	private final native int writeBytes(long portHandle, byte[] buffer, long bytesToWrite);	// Write bytes to serial port

	/**
	 * Returns the number of bytes available without blocking if {@link #readBytes} were to be called immediately
	 * after this method returns.
	 *
	 * @return The number of bytes currently available to be read, or -1 if the port is not open.
	 */
	public final int bytesAvailable() { return bytesAvailable(portHandle); }
	
	/**
	 * Returns the number of bytes still waiting to be written in the device's output queue.
	 * <p>
	 * Note that this method is not required or guaranteed to be implemented by the underlying device driver. Use it carefully and test your application to ensure it is working as you expect.
	 *
	 * @return The number of bytes currently waiting to be written, or -1 if the port is not open.
	 */
	public final int bytesAwaitingWrite() { return bytesAwaitingWrite(portHandle); }

	/**
	 * Reads up to <i>bytesToRead</i> raw data bytes from the serial port and stores them in the buffer.
	 * <p>
	 * The length of the byte buffer must be greater than or equal to the value passed in for <i>bytesToRead</i>
	 * <p>
	 * If no timeouts were specified or the read timeout was set to 0, this call will block until <i>bytesToRead</i> bytes of data have been successfully read from the serial port.
	 * Otherwise, this method will return after <i>bytesToRead</i> bytes of data have been read or the number of milliseconds specified by the read timeout have elapsed,
	 * whichever comes first, regardless of the availability of more data.
	 *
	 * @param buffer The buffer into which the raw data is read.
	 * @param bytesToRead The number of bytes to read from the serial port.
	 * @return The number of bytes successfully read, or -1 if there was an error reading from the port.
	 */
	public final int readBytes(byte[] buffer, long bytesToRead) { return readBytes(portHandle, buffer, bytesToRead); }

	/**
	 * Writes up to <i>bytesToWrite</i> raw data bytes from the buffer parameter to the serial port.
	 * <p>
	 * The length of the byte buffer must be greater than or equal to the value passed in for <i>bytesToWrite</i>
	 * <p>
	 * If no timeouts were specified or the write timeout was set to 0, this call will block until <i>bytesToWrite</i> bytes of data have been successfully written the serial port.
	 * Otherwise, this method will return after <i>bytesToWrite</i> bytes of data have been written or the number of milliseconds specified by the write timeout have elapsed,
	 * whichever comes first, regardless of the availability of more data.
	 *
	 * @param buffer The buffer containing the raw data to write to the serial port.
	 * @param bytesToWrite The number of bytes to write to the serial port.
	 * @return The number of bytes successfully written, or -1 if there was an error writing to the port.
	 */
	public final int writeBytes(byte[] buffer, long bytesToWrite) { return writeBytes(portHandle, buffer, bytesToWrite); }

	// Default Constructor
	private SerialPort() {}

	/**
	 * Adds a {@link SerialPortDataListener} to the serial port interface.
	 * <p>
	 * Calling this function enables event-based serial port callbacks to be used instead of, or in addition to, direct serial port read/write calls or the {@link java.io.InputStream}/{@link java.io.OutputStream} interface.
	 * <p>
	 * The parameter passed into this method must be an implementation of either the {@link SerialPortDataListener} or the {@link SerialPortPacketListener}.
	 * The {@link SerialPortPacketListener} interface <b>must</b> be used if you plan to use event-based reading of <i>full</i> data packets over the serial port.
	 * Otherwise, the simpler {@link SerialPortDataListener} may be used.
	 * <p>
	 * Only one listener can be registered at a time; however, that listener can be used to detect multiple types of serial port events.
	 * Refer to {@link SerialPortDataListener} and {@link SerialPortPacketListener} for more information.
	 *
	 * @param listener A {@link SerialPortDataListener} or {@link SerialPortPacketListener}implementation to be used for event-based serial port communications.
	 * @return Whether the listener was successfully registered with the serial port.
	 * @see SerialPortDataListener
	 * @see SerialPortPacketListener
	 */
	public final boolean addDataListener(SerialPortDataListener listener)
	{
		if (userDataListener != null)
			return false;
		userDataListener = listener;
		serialEventListener = new SerialPortEventListener((userDataListener instanceof SerialPortPacketListener) ?
				((SerialPortPacketListener)userDataListener).getPacketSize() : 0);

		eventFlags = 0;
		if ((listener.getListeningEvents() & LISTENING_EVENT_DATA_AVAILABLE) > 0)
			eventFlags |= LISTENING_EVENT_DATA_AVAILABLE;
		if ((listener.getListeningEvents() & LISTENING_EVENT_DATA_RECEIVED) > 0)
			eventFlags |= LISTENING_EVENT_DATA_RECEIVED;
		if ((listener.getListeningEvents() & LISTENING_EVENT_DATA_WRITTEN) > 0)
			eventFlags |= LISTENING_EVENT_DATA_WRITTEN;

		if (isOpened)
		{
			configEventFlags(portHandle);
			serialEventListener.startListening();
		}
		return true;
	}

	/**
	 * Removes the associated {@link SerialPortDataListener} from the serial port interface.
	 */
	public final void removeDataListener()
	{
		if (serialEventListener != null)
		{
			serialEventListener.stopListening();
			serialEventListener = null;
		}
		userDataListener = null;

		eventFlags = 0;
		if (isOpened)
			configEventFlags(portHandle);
	}

	/**
	 * Returns an {@link java.io.InputStream} object associated with this serial port.
	 * <p>
	 * Allows for easier read access of the underlying data stream and abstracts away many low-level read details.
	 * <p>
	 * Make sure to call the {@link java.io.InputStream#close()} method when you are done using this stream.
	 *
	 * @return An {@link java.io.InputStream} object associated with this serial port.
	 * @see java.io.InputStream
	 */
	public final InputStream getInputStream() { return inputStream; }

	/**
	 * Returns an {@link java.io.OutputStream} object associated with this serial port.
	 * <p>
	 * Allows for easier write access to the underlying data stream and abstracts away many low-level writing details.
	 * <p>
	 * Make sure to call the {@link java.io.OutputStream#close()} method when you are done using this stream.
	 *
	 * @return An {@link java.io.OutputStream} object associated with this serial port.
	 * @see java.io.OutputStream
	 */
	public final OutputStream getOutputStream() { return outputStream; }

	/**
	 * Sets all serial port parameters at one time.
	 * <p>
	 * Allows the user to set all port parameters with a single function call.
	 * <p>
	 * The baud rate can be any arbitrary value specified by the user.  The default value is 9600 baud.  The data bits parameter
	 * specifies how many data bits to use per word.  The default is 8, but any values from 5 to 8 are acceptable.
	 * <p>
	 * The default number of stop bits is 1, but 2 bits can also be used or even 1.5 on Windows machines.  Please use the built-in
	 * constants for this parameter ({@link #ONE_STOP_BIT}, {@link #ONE_POINT_FIVE_STOP_BITS}, {@link #TWO_STOP_BITS}).
	 * <p>
	 * The parity parameter specifies how error detection is carried out.  Again, the built-in constants should be used.
	 * Acceptable values are {@link #NO_PARITY}, {@link #EVEN_PARITY}, {@link #ODD_PARITY}, {@link #MARK_PARITY}, and {@link #SPACE_PARITY}.
	 *
	 * @param newBaudRate The desired baud rate for this serial port.
	 * @param newDataBits The number of data bits to use per word.
	 * @param newStopBits The number of stop bits to use.
	 * @param newParity The type of parity error-checking desired.
	 * @see #ONE_STOP_BIT
	 * @see #ONE_POINT_FIVE_STOP_BITS
	 * @see #TWO_STOP_BITS
	 * @see #NO_PARITY
	 * @see #EVEN_PARITY
	 * @see #ODD_PARITY
	 * @see #MARK_PARITY
	 * @see #SPACE_PARITY
	 */
	public final void setComPortParameters(int newBaudRate, int newDataBits, int newStopBits, int newParity)
	{
		baudRate = newBaudRate;
		dataBits = newDataBits;
		stopBits = newStopBits;
		parity = newParity;

		if (isOpened)
		{
			try { Thread.sleep(200); } catch (Exception e) {}
			configPort(portHandle);
		}
	}

	/**
	 * Sets the serial port read and write timeout parameters.
	 * <p>
	 * <i>Note that write timeouts are only available on Windows-based systems. There is no functionality to set a write timeout on other operating systems.</i>
	 * <p>
	 * The built-in timeout mode constants should be used ({@link #TIMEOUT_NONBLOCKING}, {@link #TIMEOUT_READ_SEMI_BLOCKING},
	 * {@link #TIMEOUT_WRITE_SEMI_BLOCKING}, {@link #TIMEOUT_READ_BLOCKING}, {@link #TIMEOUT_WRITE_BLOCKING}, {@link #TIMEOUT_SCANNER}) to specify how
	 * timeouts are to be handled.
	 * <p>
	 * Valid modes are:
	 * <p>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Non-blocking: {@link #TIMEOUT_NONBLOCKING}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Read Semi-blocking: {@link #TIMEOUT_READ_SEMI_BLOCKING}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Read/Write Semi-blocking: {@link #TIMEOUT_READ_SEMI_BLOCKING} | {@link #TIMEOUT_WRITE_SEMI_BLOCKING}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Read Semi-Blocking/Write Full-blocking: {@link #TIMEOUT_READ_SEMI_BLOCKING} | {@link #TIMEOUT_WRITE_BLOCKING}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Read Full-blocking: {@link #TIMEOUT_READ_BLOCKING}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Read Full-blocking/Write Semi-blocking: {@link #TIMEOUT_READ_BLOCKING} | {@link #TIMEOUT_WRITE_SEMI_BLOCKING}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Read/Write Full-blocking: {@link #TIMEOUT_READ_BLOCKING} | {@link #TIMEOUT_WRITE_BLOCKING}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Scanner: {@link #TIMEOUT_SCANNER}<br>
	 * <p>
	 * The {@link #TIMEOUT_NONBLOCKING} mode specifies that the corresponding {@link #readBytes(byte[],long)} or {@link #writeBytes(byte[],long)} call
	 * will return immediately with any available data.
	 * <p>
	 * The {@link #TIMEOUT_READ_SEMI_BLOCKING} or {@link #TIMEOUT_WRITE_SEMI_BLOCKING} modes specify that the corresponding calls will block until either
	 * <i>newReadTimeout</i> or <i>newWriteTimeout</i> milliseconds of inactivity have elapsed or at least 1 byte of data can be written or read.
	 * <p>
	 * The {@link #TIMEOUT_READ_BLOCKING} or {@link #TIMEOUT_WRITE_BLOCKING} modes specify that the corresponding call will block until either
	 * <i>newReadTimeout</i> or <i>newWriteTimeout</i> milliseconds have elapsed since the start of the call or the total number of requested bytes can be written or
	 * returned.
	 * <p>
	 * The {@link #TIMEOUT_SCANNER} mode is intended for use with the Java {@link java.util.Scanner} class for reading from the serial port. In this mode,
	 * manually specified timeouts are ignored to ensure compatibility with the Java specification.
	 * <p>
	 * A value of 0 for either <i>newReadTimeout</i> or <i>newWriteTimeout</i> indicates that a {@link #readBytes(byte[],long)} or
	 * {@link #writeBytes(byte[],long)} call should block forever until it can return successfully (based upon the current timeout mode specified).
	 * <p>
	 * It is important to note that non-Windows operating systems only allow decisecond (1/10th of a second) granularity for serial port timeouts. As such, your
	 * millisecond timeout value will be rounded to the nearest decisecond under Linux or Mac OS. To ensure consistent performance across multiple platforms, it is
	 * advisable that you set your timeout values to be multiples of 100, although this is not strictly enforced.
	 *
	 * @param newTimeoutMode The new timeout mode as specified above.
	 * @param newReadTimeout The number of milliseconds of inactivity to tolerate before returning from a {@link #readBytes(byte[],long)} call.
	 * @param newWriteTimeout The number of milliseconds of inactivity to tolerate before returning from a {@link #writeBytes(byte[],long)} call.
	 */
	public final void setComPortTimeouts(int newTimeoutMode, int newReadTimeout, int newWriteTimeout)
	{
		timeoutMode = newTimeoutMode;
		if (isWindows)
		{
			readTimeout = newReadTimeout;
			writeTimeout = newWriteTimeout;
		}
		else if ((newReadTimeout > 0) && (newReadTimeout <= 100))
			readTimeout = 100;
		else
			readTimeout = Math.round((float)newReadTimeout / 100.0f) * 100;

		if (isOpened)
		{
			try { Thread.sleep(200); } catch (Exception e) {}
			configTimeouts(portHandle);
		}
	}

	/**
	 * Sets the desired baud rate for this serial port.
	 * <p>
	 * The default baud rate is 9600 baud.
	 *
	 * @param newBaudRate The desired baud rate for this serial port.
	 */
	public final void setBaudRate(int newBaudRate)
	{
		baudRate = newBaudRate;

		if (isOpened)
		{
			try { Thread.sleep(200); } catch (Exception e) {}
			configPort(portHandle);
		}
	}

	/**
	 * Sets the desired number of data bits per word.
	 * <p>
	 * The default number of data bits per word is 8.
	 *
	 * @param newDataBits The desired number of data bits per word.
	 */
	public final void setNumDataBits(int newDataBits)
	{
		dataBits = newDataBits;

		if (isOpened)
		{
			try { Thread.sleep(200); } catch (Exception e) {}
			configPort(portHandle);
		}
	}

	/**
	 * Sets the desired number of stop bits per word.
	 * <p>
	 * The default number of stop bits per word is 1.  Built-in stop-bit constants should be used
	 * in this method ({@link #ONE_STOP_BIT}, {@link #ONE_POINT_FIVE_STOP_BITS}, {@link #TWO_STOP_BITS}).
	 * <p>
	 * Note that {@link #ONE_POINT_FIVE_STOP_BITS} stop bits may not be available on non-Windows systems.
	 *
	 * @param newStopBits The desired number of stop bits per word.
	 * @see #ONE_STOP_BIT
	 * @see #ONE_POINT_FIVE_STOP_BITS
	 * @see #TWO_STOP_BITS
	 */
	public final void setNumStopBits(int newStopBits)
	{
		stopBits = newStopBits;

		if (isOpened)
		{
			try { Thread.sleep(200); } catch (Exception e) {}
			configPort(portHandle);
		}
	}

	/**
	 * Specifies what kind of flow control to enable for this serial port.
	 * <p>
	 * By default, no flow control is enabled.  Built-in flow control constants should be used
	 * in this method ({@link #FLOW_CONTROL_RTS_ENABLED}, {@link #FLOW_CONTROL_CTS_ENABLED}, {@link #FLOW_CONTROL_DTR_ENABLED},
	 * {@link #FLOW_CONTROL_DSR_ENABLED}, {@link #FLOW_CONTROL_XONXOFF_IN_ENABLED}, {@link #FLOW_CONTROL_XONXOFF_OUT_ENABLED}), and can be OR'ed together.
	 * <p>
	 * Valid flow control configurations are:
	 * <p>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;None: {@link #FLOW_CONTROL_DISABLED}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;CTS: {@link #FLOW_CONTROL_CTS_ENABLED}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;RTS/CTS: {@link #FLOW_CONTROL_RTS_ENABLED} | {@link #FLOW_CONTROL_CTS_ENABLED}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;DSR: {@link #FLOW_CONTROL_DSR_ENABLED}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;DTR/DSR: {@link #FLOW_CONTROL_DTR_ENABLED} | {@link #FLOW_CONTROL_DSR_ENABLED}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;XOn/XOff: {@link #FLOW_CONTROL_XONXOFF_IN_ENABLED} | {@link #FLOW_CONTROL_XONXOFF_OUT_ENABLED}
	 * <p>
	 * Note that only one valid flow control configuration can be used at any time.  For example, attempting to use both XOn/XOff
	 * <b>and</b> RTS/CTS will most likely result in an unusable serial port.
	 * <p>
	 * Also note that some flow control modes are only available on certain operating systems. Valid modes for each OS are:
	 * <p>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Windows: CTS, RTS/CTS, DSR, DTR/DSR, Xon, Xoff, Xon/Xoff<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Mac: RTS/CTS, Xon, Xoff, Xon/Xoff<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Linux: RTS/CTS, Xon, Xoff, Xon/Xoff
	 *
	 * @param newFlowControlSettings The desired type of flow control to enable for this serial port.
	 * @see #FLOW_CONTROL_DISABLED
	 * @see #FLOW_CONTROL_RTS_ENABLED
	 * @see #FLOW_CONTROL_CTS_ENABLED
	 * @see #FLOW_CONTROL_DTR_ENABLED
	 * @see #FLOW_CONTROL_DSR_ENABLED
	 * @see #FLOW_CONTROL_XONXOFF_IN_ENABLED
	 * @see #FLOW_CONTROL_XONXOFF_OUT_ENABLED
	 */
	public final void setFlowControl(int newFlowControlSettings)
	{
		flowControl = newFlowControlSettings;

		if (isOpened)
		{
			try { Thread.sleep(200); } catch (Exception e) {}
			configPort(portHandle);
		}
	}

	/**
	 * Sets the desired parity error-detection scheme to be used.
	 * <p>
	 * The parity parameter specifies how error detection is carried out.  The built-in parity constants should be used.
	 * Acceptable values are {@link #NO_PARITY}, {@link #EVEN_PARITY}, {@link #ODD_PARITY}, {@link #MARK_PARITY}, and {@link #SPACE_PARITY}.
	 *
	 * @param newParity The desired parity scheme to be used.
	 * @see #NO_PARITY
	 * @see #EVEN_PARITY
	 * @see #ODD_PARITY
	 * @see #MARK_PARITY
	 * @see #SPACE_PARITY
	 */
	public final void setParity(int newParity)
	{
		parity = newParity;

		if (isOpened)
		{
			try { Thread.sleep(200); } catch (Exception e) {}
			configPort(portHandle);
		}
	}

	/**
	 * Gets a descriptive string representing this serial port or the device connected to it.
	 * <p>
	 * This description is generated by the operating system and may or may not be a good representation of the actual port or
	 * device it describes.
	 *
	 * @return A descriptive string representing this serial port.
	 */
	public final String getDescriptivePortName() { return portString.trim(); }

	/**
	 * Gets the operating system-defined device name corresponding to this serial port.
	 *
	 * @return The system-defined device name of this serial port.
	 */
	public final String getSystemPortName() { return (isWindows ? comPort.substring(comPort.lastIndexOf('\\')+1) : comPort.substring(comPort.lastIndexOf('/')+1)); }

	/**
	 * Gets the current baud rate of the serial port.
	 *
	 * @return The current baud rate of the serial port.
	 */
	public final int getBaudRate() { return baudRate; }

	/**
	 * Gets the current number of data bits per word.
	 *
	 * @return The current number of data bits per word.
	 */
	public final int getNumDataBits() { return dataBits; }

	/**
	 * Gets the current number of stop bits per word.
	 * <p>
	 * The return value should not be interpreted as an integer, but rather compared with the built-in stop bit constants
	 * ({@link #ONE_STOP_BIT}, {@link #ONE_POINT_FIVE_STOP_BITS}, {@link #TWO_STOP_BITS}).
	 *
	 * @return The current number of stop bits per word.
	 * @see #ONE_STOP_BIT
	 * @see #ONE_POINT_FIVE_STOP_BITS
	 * @see #TWO_STOP_BITS
	 */
	public final int getNumStopBits() { return stopBits; }

	/**
	 * Gets the current parity error-checking scheme.
	 * <p>
	 * The return value should not be interpreted as an integer, but rather compared with the built-in parity constants
	 * ({@link #NO_PARITY}, {@link #EVEN_PARITY}, {@link #ODD_PARITY}, {@link #MARK_PARITY}, and {@link #SPACE_PARITY}).
	 *
	 * @return The current parity scheme.
	 * @see #NO_PARITY
	 * @see #EVEN_PARITY
	 * @see #ODD_PARITY
	 * @see #MARK_PARITY
	 * @see #SPACE_PARITY
	 */
	public final int getParity() { return parity; }

	/**
	 * Gets the number of milliseconds of inactivity to tolerate before returning from a {@link #readBytes(byte[],long)} call.
	 * <p>
	 * A value of 0 indicates that a {@link #readBytes(byte[],long)} call will block forever until it has successfully read
	 * the indicated number of bytes from the serial port.
	 * <p>
	 * Any value other than 0 indicates the number of milliseconds of inactivity that will be tolerated before the {@link #readBytes(byte[],long)}
	 * call will return.
	 *
	 * @return The number of milliseconds of inactivity to tolerate before returning from a {@link #readBytes(byte[],long)} call.
	 */
	public final int getReadTimeout() { return readTimeout; }

	/**
	 * Gets the number of milliseconds of inactivity to tolerate before returning from a {@link #writeBytes(byte[],long)} call.
	 * <p>
	 * A value of 0 indicates that a {@link #writeBytes(byte[],long)} call will block forever until it has successfully written
	 * the indicated number of bytes to the serial port.
	 * <p>
	 * Any value other than 0 indicates the number of milliseconds of inactivity that will be tolerated before the {@link #writeBytes(byte[],long)}
	 * call will return.
	 * <p>
	 * Note that write timeouts are only available on Windows operating systems. This value is ignored on all other systems.
	 *
	 * @return The number of milliseconds of inactivity to tolerate before returning from a {@link #writeBytes(byte[],long)} call.
	 */
	public final int getWriteTimeout() { return writeTimeout; }

	/**
	 * Returns the flow control settings enabled on this serial port.
	 * <p>
	 * The integer result should be masked with the built-in flow control constants to test if the desired setting is enabled.
	 * Valid flow control configurations are:
	 * <p>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;None: {@link #FLOW_CONTROL_DISABLED}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;CTS: {@link #FLOW_CONTROL_CTS_ENABLED}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;RTS/CTS: {@link #FLOW_CONTROL_RTS_ENABLED} | {@link #FLOW_CONTROL_CTS_ENABLED}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;DSR: {@link #FLOW_CONTROL_DSR_ENABLED}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;DTR/DSR: {@link #FLOW_CONTROL_DTR_ENABLED} | {@link #FLOW_CONTROL_DSR_ENABLED}<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;XOn/XOff: {@link #FLOW_CONTROL_XONXOFF_IN_ENABLED} | {@link #FLOW_CONTROL_XONXOFF_OUT_ENABLED}
	 * <p>
	 * Note that some flow control modes are only available on certain operating systems. Valid modes for each OS are:
	 * <p>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Windows: CTS, RTS/CTS, DSR, DTR/DSR, Xon, Xoff, Xon/Xoff<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Mac: RTS/CTS, Xon, Xoff, Xon/Xoff<br>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Linux: RTS/CTS, Xon, Xoff, Xon/Xoff
	 *
	 * @return The flow control settings enabled on this serial port.
	 * @see #FLOW_CONTROL_DISABLED
	 * @see #FLOW_CONTROL_RTS_ENABLED
	 * @see #FLOW_CONTROL_CTS_ENABLED
	 * @see #FLOW_CONTROL_DTR_ENABLED
	 * @see #FLOW_CONTROL_DSR_ENABLED
	 * @see #FLOW_CONTROL_XONXOFF_IN_ENABLED
	 * @see #FLOW_CONTROL_XONXOFF_OUT_ENABLED
	 */
	public final int getFlowControlSettings() { return flowControl; }

	// Private EventListener class
	private final class SerialPortEventListener
	{
		private volatile boolean isListening = false;
		private final byte[] dataPacket;
		private volatile int dataPacketIndex = 0;
		private Thread serialEventThread = null;

		public SerialPortEventListener(int packetSizeToReceive) { dataPacket = new byte[packetSizeToReceive]; }

		public final void startListening()
		{
			if (isListening)
				return;
			isListening = true;

			dataPacketIndex = 0;
			serialEventThread = new Thread(new Runnable() {
				@Override
				public void run()
				{
					while (isListening && isOpened) { try { waitForSerialEvent(); } catch (NullPointerException e) { isListening = false; } }
					isListening = false;
				}
			});
			serialEventThread.start();
		}

		public final void stopListening()
		{
			if (!isListening)
				return;
			isListening = false;

			int oldEventFlags = eventFlags;
			eventFlags = 0;
			configEventFlags(portHandle);
			try { serialEventThread.join(); } catch (InterruptedException e) {}
			serialEventThread = null;
			eventFlags = oldEventFlags;
		}

		public final void waitForSerialEvent() throws NullPointerException
		{
			switch (waitForEvent(portHandle))
			{
				case LISTENING_EVENT_DATA_AVAILABLE:
				{
					if ((eventFlags & LISTENING_EVENT_DATA_RECEIVED) > 0)
					{
						// Read data from serial port
						int numBytesAvailable, bytesRemaining, newBytesIndex;
						while ((numBytesAvailable = bytesAvailable(portHandle)) > 0)
						{
							byte[] newBytes = new byte[numBytesAvailable];
							newBytesIndex = 0;
							bytesRemaining = readBytes(portHandle, newBytes, newBytes.length);
							while (bytesRemaining >= (dataPacket.length - dataPacketIndex))
							{
								System.arraycopy(newBytes, newBytesIndex, dataPacket, dataPacketIndex, dataPacket.length - dataPacketIndex);
								bytesRemaining -= (dataPacket.length - dataPacketIndex);
								newBytesIndex += (dataPacket.length - dataPacketIndex);
								dataPacketIndex = 0;
								userDataListener.serialEvent(new SerialPortEvent(SerialPort.this, LISTENING_EVENT_DATA_RECEIVED, dataPacket.clone()));
							}
							if (bytesRemaining > 0)
							{
								System.arraycopy(newBytes, newBytesIndex, dataPacket, dataPacketIndex, bytesRemaining);
								dataPacketIndex += bytesRemaining;
							}
						}
					}
					else if ((eventFlags & LISTENING_EVENT_DATA_AVAILABLE) > 0)
						userDataListener.serialEvent(new SerialPortEvent(SerialPort.this, LISTENING_EVENT_DATA_AVAILABLE));
					break;
				}
				case LISTENING_EVENT_DATA_WRITTEN:
				{
					userDataListener.serialEvent(new SerialPortEvent(SerialPort.this, LISTENING_EVENT_DATA_WRITTEN));
					break;
				}
				default:
					break;
			}
		}
	}

	// InputStream interface class
	private final class SerialPortInputStream extends InputStream
	{
		public SerialPortInputStream() {}

		@Override
		public final int available() throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");

			return bytesAvailable(portHandle);
		}

		@Override
		public final int read() throws IOException
		{
			byte[] buffer = new byte[1];
			int bytesRead;

			while (isOpened)
			{
				bytesRead = readBytes(portHandle, buffer, 1l);
				if (bytesRead > 0)
					return ((int)buffer[0] & 0x000000FF);
				try { Thread.sleep(1); } catch (Exception e) {}
			}
			throw new IOException("This port appears to have been shutdown or disconnected.");
		}

		@Override
		public final int read(byte[] b) throws IOException
		{
			return read(b, 0, b.length);
		}

		@Override
		public final int read(byte[] b, int off, int len) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			if (len == 0)
				return 0;

			byte[] buffer = new byte[len];
			int numRead = readBytes(portHandle, buffer, len);
			if (numRead > 0)
				System.arraycopy(buffer, 0, b, off, numRead);

			return numRead;
		}

		@Override
		public final long skip(long n) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");

			byte[] buffer = new byte[(int)n];
			return readBytes(portHandle, buffer, n);
		}
	}

	// OutputStream interface class
	private final class SerialPortOutputStream extends OutputStream
	{
		public SerialPortOutputStream() {}

		@Override
		public final void write(int b) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");

			byte[] buffer = new byte[1];
			buffer[0] = (byte)(b & 0x000000FF);
			if (writeBytes(portHandle, buffer, 1l) < 0)
				throw new IOException("This port appears to have been shutdown or disconnected.");
		}

		@Override
		public final void write(byte[] b) throws IOException
		{
			write(b, 0, b.length);
		}

		@Override
		public final void write(byte[] b, int off, int len) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");

			byte[] buffer = new byte[len];
			System.arraycopy(b, off, buffer, 0, len);
			if (writeBytes(portHandle, buffer, len) < 0)
				throw new IOException("This port appears to have been shutdown or disconnected.");
		}
	}
}
