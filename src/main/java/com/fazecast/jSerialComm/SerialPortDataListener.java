package com.fazecast.jSerialComm;

import java.util.EventListener;

public interface SerialPortDataListener extends EventListener
{
	// TODO: Documentation - can OR together desired listening events
	// Reading vs data available precedence
	public abstract int getListeningEvents();
	
	// TODO: Documentation
	public abstract void serialEvent(SerialPortEvent event);
}
