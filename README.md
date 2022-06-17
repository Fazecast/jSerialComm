# jSerialComm

_A platform-independent serial port access library for Java._


## Usage

For usage examples, please refer to the [Usage wiki](https://github.com/Fazecast/jSerialComm/wiki/Usage-Examples).

If you intend to use the library in multiple applications simultaneously, please make sure
to set the ```fazecast.jSerialComm.appid``` property before accessing the SerialPort class
so that applications don't accidentally delete each others' temporary files during boot-up:
```
System.setProperty("fazecast.jSerialComm.appid", "YOUR_APPLICATION_IDENTIFIER")
```

In order to use the ```jSerialComm``` library in your own project, you must simply
include the JAR file in your build path and import it like any other
Java package using ```import com.fazecast.jSerialComm.*;```.

Alternatively, you can automatically add ```jSerialComm``` to your project as a
dependency from the ```Maven Central Repository```. Use the following dependency
declaration depending on your build system:

* Maven:

```
<dependency>
  <groupId>com.fazecast</groupId>
  <artifactId>jSerialComm</artifactId>
  <version>[2.0.0,3.0.0)</version>
</dependency>
```

* Ivy:

```
<dependency org="com.fazecast" name="jSerialComm" rev="[2.0.0,3.0.0)"/>
```

* Groovy:

```
@Grab(group='com.fazecast', module='jSerialComm', version='[2.0.0,3.0.0)')
```

* Gradle:

```
compile 'com.fazecast:jSerialComm:[2.0.0,3.0.0)'
```

* Gradle (.kts):

```
compile("com.fazecast:jSerialComm:[2.0.0,3.0.0)")
```

* Buildr:

```
compile.with 'com.fazecast:jSerialComm:jar:[2.0.0,3.0.0)'
```

* Scala/SBT:

```
libraryDependencies += "com.fazecast" % "jSerialComm" % "[2.0.0,3.0.0)"
```

* Leiningen:

```
[com.fazecast/jSerialComm "[2.0.0,3.0.0)"]
```

Finally, if you are working on a device that provides no other means of allowing temporary native files to run, jSerialComm supports loading a pre-extracted version of its native library from a user-defined location using the startup flag: `-DjSerialComm.library.path="<LIB_PATH>"`, where `LIB_PATH` can either be a directory containing the single native jSerialComm library for your correct architecture or the entire extracted arch-specific directory structure from inside the jSerialComm JAR file; however, this should be used as a last resort as it makes versioning and upgrading much more difficult and error-prone.


## Troubleshooting

If you are using Linux and this library does not appear to be working, ensure
that you have the correct permissions set to access the serial port on your system.
One way to test this is to run your application as root or by using the
```sudo``` command. If everything works, you will need to either run your
application as ```root``` in the future or fix the permissions on your system.
For further instructions, refer to the [Troubleshooting wiki](https://github.com/Fazecast/jSerialComm/wiki/Troubleshooting).

On some very few systems which use custom ARM-based CPUs and/or have extremely
restrictive permissions, the library may be unable to determine that the
underlying system architecture is ARM. In this case, you can force the
library to disable its auto-detect functionality and instead directly specify
the architecture using the Java ```os.arch_full``` system property. Acceptable
values for this property are currently one of: ``armv5``, ``armv6``,
``armv6-hf``, ``armv7``, ``armv7-hf``, ``armv8_32``, ``armv8_64``, ``ppc64le``,
``x86``, or ``x86_64``.

Additionally, some systems may block execution of libraries from the system
temp folder. If you are experiencing this problem, you can specify a different,
less restrictive temp folder by adding
```System.setProperty("java.io.tmpdir", "/folder/where/execution/is/allowed")```
to your program before the first use of this library. When doing this, make sure
that the folder you specify already exists and has the correct permissions set
to allow execution of a shared library.

Optionally, the same result can be achieved by running your Java application
from the command line and specifying the `java.io.tmpdir` directory as an
additional parameter,
e.g.: ```java -Djava.io.tmpdir=/folder/of/your/choice -jar yourApplication.jar```

On Windows, you may be able to achieve the same result by setting the TMP
environment variable (either through the Settings->System Properties->Environment
Variables GUI or via ```SET TMP=C:\Desired\Tmp\Folder``` in a command terminal),
although setting this variable through Java is preferable when possible.

An additional note for Linux users:  If you are operating this library in
event-based mode, the ```LISTENING_EVENT_DATA_WRITTEN``` event will never occur.
This is not a bug, but rather a limitation of the Linux operating system.

For other troubleshooting issues, please see if you can find an answer in either
the [Usage-Examples wiki](https://github.com/Fazecast/jSerialComm/wiki/Usage-Examples)
or the [Troubleshooting Wiki](https://github.com/Fazecast/jSerialComm/wiki/Troubleshooting).

If your question is still not answered, feel free to open an issue report on
this project's [GitHub page](https://github.com/Fazecast/jSerialComm/issues),
and we will be glad to look into it.


## Building

Building this library yourself is not advised (at least not for distribution)
since it requires native compilation across multiple platforms. It is
recommended to simply use the pre-built ```jSerialComm``` library in your
application. For installation/usage instructions, please skip to the [usage](#usage)
section.

If you do choose to build this library for your specific system, please follow
the instructions in the [Building Tutorial](https://github.com/Fazecast/jSerialComm/wiki/Building-Tutorial)
to set up the required native cross-compilation toolchains.
