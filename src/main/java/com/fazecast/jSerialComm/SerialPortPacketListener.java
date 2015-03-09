package com.fazecast.jSerialComm;

public interface SerialPortPacketListener extends SerialPortDataListener
{
	public abstract int getPacketSize();
}
