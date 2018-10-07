# jSerialComm-windows

This is a fork of Fazecast/jSerialComm v2.2.3, a good Java library for interacting with serial ports on a number of different platforms.

jSerialComm v2.2.3 does not provide functionality to set a serial port's OS-level internal buffer. On *Windows* this becomes a problem if the receiving side of the port transmission is poorly implemented in that it only inspects the "available" number of bytes rather than draining available bytes into another buffer and waiting for the port's buffer to be refilled. I had the misfortune of being trapped in such a situation with 20-year old hardware that expected 1536 bytes to be available at the same time, but Windows defaulted to 1282 bytes. Whoops.

This fork of jSerialComm just adds the ability to set the OS-level port buffer size on Windows. That's it. It looks like similar functionality exists at the OS level for Linux and Android, but I don't have the equipment or the need to test those changes. I've submitted a [pull request](https://docs.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-setcommstate) to the Fazecast/jSerialComm project but since it only contains changes for the Windows serial port driver it probably won't be accepted. If you need to set port buffer size on Windows, you can use this version of the library instead. It adds a call to the Win32 API's [SetCommState](https://docs.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-setcommstate) function.

## Usage

Use jSerialComm-windows the same as you would the original jSerialComm. The only difference is the addition of two methods:
```SerialPort.setSendBufferSize(int)``` and 
```SerialPort.setReceiveBufferSize(int)``` .

The default port buffer size is set to 4096 bytes.

At this time the jSerialComm-windows library is only available as a JAR download that you include in your project. If you'd like to see it on Maven Central/jCenter/Ivy/whatever, I'll happily merge a pull request. For that matter you can just fork the repo and stick it in your dependency management repository of your choice. If you do, please consider sending me a link so I can kill this repo and use yours instead.

For usage examples of the original jSerialComm library, please refer to the [Usage wiki](https://github.com/Fazecast/jSerialComm/wiki/Usage-Examples).
