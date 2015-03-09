package com.fazecast.jSerialComm;

import java.util.EventObject;

public final class SerialPortEvent extends EventObject
{
	private static final long serialVersionUID = 3060830619653354150L;
	private final int eventType;
	private final byte[] serialData;

	public SerialPortEvent(Object source, int serialEventType)
	{
		super(source);
		eventType = serialEventType;
		serialData = null;
	}
	
	public SerialPortEvent(Object source, int serialEventType, byte[] data)
	{
		super(source);
		eventType = serialEventType;
		serialData = data;
	}
	
	public final int getEventType() { return eventType; }
	public final byte[] getReceivedData() { return serialData; }

	//TODO: Implementations:  DataAvailableListener(void), return how much data is available
	
	//TODO: Implementations:  DataReceivedListener(Set amount of data to read before notifying)
	//Should not mix DataReceivedListener with direct reads (read(), readBytes(), etc) or with InputStream usage
	//Note: Using DataReceivedListener will negate any COM port read timeout settings since they make no sense in this context
	
	//TODO: Implementations:  DataWrittenListener(Return with number of bytes written)
}
