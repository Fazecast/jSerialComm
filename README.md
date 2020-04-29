# jSerialComm

_A platform-independent serial port access library for Java._


## Usage

For usage examples, please refer to the [Usage wiki](https://github.com/Fazecast/jSerialComm/wiki/Usage-Examples).

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
``armv6-hf``, ``armv7``, ``armv7-hf``, ``armv8_32``, ``armv8_64``,
``x86``, or ``x86_64``.

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
application. For installation/usage instructions, please skip to the [usage](#usage) section.

If you do choose to build this library for your specific system, the recommended
methodology is to use [Gradle](https://gradle.org/) coupled with two
Java SDKs, [version 1.6](https://www.oracle.com/java/technologies/javase-java-archive-javase6-downloads.html)
and [version 9.0.4](https://www.oracle.com/java/technologies/javase/javase9-archive-downloads.html)
(for backward compatibility).

Once the Java SDKs have been installed, ensure that you have an environment
variable called ```JDK_HOME``` set to the base directory of your JDK 9.0.4
installation. Once this has been done, refer to the section corresponding to
your specific Operating System for further instructions.

Please note, if you would like to edit any of the source code or view it in an
IDE (such as Eclipse), you can automatically build the Eclipse project files by
entering the following on a command line or terminal from the base directory of
this project:

    gradle eclipse

You can then Import the project using the "Existing Project into Workspace" import
tool in Eclipse. (Note that if you use Eclipse as an IDE, you will probably want
to install the Eclipse CDT plugin for proper handling of the C source code).


### Linux/UNIX

Ensure that the following tools are installed on your Linux distribution:

    # On some distros, these may be called multilib tools for gcc and binutils
    gcc make glibc-devel.x86_64 glibc-devel.i686

Ensure that the ```JDK_HOME``` environment variable has been set for the 9.0.4
version of your Java SDK. The correct directory can usually be found by entering
the following command:

    readlink -f /usr/bin/java

Export the result of this command ***up to but not including*** the
```/jre/...``` portion using the ```export``` command. For example, if
```readlink``` produced ```/usr/lib/jvm/java-9.0.4/jre/bin/java``` as an output,
the export command would look like: ```export JDK_HOME=/usr/lib/jvm/java-9.0.4```

Run the following commands:

    cd src/main/c/Posix
    make linux
    cd ../../../..
    gradle build

The resulting ```jSerialComm``` library can be found in the project directory
```build/libs``` under the name ```jSerialComm-{VERSION}.jar```.


### Solaris

Ensure that you have a cross-compiler installed on your Linux distribution
capable of compiling for both x86 and Sparc-based Solaris architectures.
Instructions for creating such a toolchain can be found on the
[Solaris Cross-Compiler wiki](https://github.com/Fazecast/jSerialComm/wiki/Building-Solaris-Cross-Compilers).

Ensure that the ```JDK_HOME``` environment variable has been set for the 9.0.4
version of your Java SDK. The correct directory can usually be found by entering
the following command:

    readlink -f /usr/bin/java

Export the result of this command ***up to but not including*** the
```/jre/...``` portion using the ```export``` command. For example, if
```readlink``` produced ```/usr/lib/jvm/java-9.0.4/jre/bin/java``` as an output,
the export command would look like: ```export JDK_HOME=/usr/lib/jvm/java-9.0.4```

Run the following commands:

    cd src/main/c/Posix
    make solaris
    cd ../../../..
    gradle build

The resulting ```jSerialComm``` library can be found in the project directory
```build/libs``` under the name ```jSerialComm-{VERSION}.jar```.


### ARM-Based Mobile Linux (non-Android)

Ensure that you have a cross-compiler installed on your Linux distribution
capable of compiling for ARM-based architectures. I prefer ```crosstool-ng```
for this purpose.

Ensure that the ```JDK_HOME``` environment variable has been set for the 9.0.4
version of your Java SDK. The correct directory can usually be found by entering
the following command:

    readlink -f /usr/bin/java

Export the result of this command ***up to but not including*** the
```/jre/...``` portion using the ```export``` command. For example, if
```readlink``` produced ```/usr/lib/jvm/java-9.0.4/jre/bin/java``` as an output,
the export command would look like: ```export JDK_HOME=/usr/lib/jvm/java-9.0.4```

Run the following commands:

    cd src/main/c/Posix
    make arm
    cd ../../../..
    gradle build

The resulting ```jSerialComm``` library can be found in the project directory
```build/libs``` under the name ```jSerialComm-{VERSION}.jar```.


### Android

Ensure that the Android NDK is installed on your system. For purposes of these
instructions, assume that it is installed at ```{NDK_HOME}```.

Run the following commands in order:

    cd src/main/c/Android
    {NDK_HOME}/ndk-build
    cd ../../../..
    gradle build

The resulting ```jSerialComm``` library can be found in the project directory
```build/libs``` under the name ```jSerialComm-{VERSION}.jar```


### Mac OS X

Ensure that [Xcode](https://developer.apple.com/xcode/) is installed on your system.
If it is not, it can be downloaded via the App Store. You must also make sure
that the ```Xcode Command Line Tools``` are installed. This can be done by
entering the following command in a terminal: ```xcode-select --install```

Run the following commands in order:

    cd src/main/c/Posix
    make osx
    cd ../../../..
    gradle build

The resulting ```jSerialComm``` library can be found in the project directory
```build/libs``` under the name ```jSerialComm-{VERSION}.jar```


### Windows

Ensure that the [Visual Studio C++ Compiler](https://www.visualstudio.com/) is
installed on your system.

On Windows, the Visual Studio Compiler must be configured to build either
32- or 64-bit binaries but never both at the same time. Therefore, you will have
to build binaries for the two architectures separately.

Open a command prompt and run the following command:

    SET VC_DIRECTORY="C:\Program Files (x86)\Microsoft Visual Studio [version]\VC"

where ```[version]``` matches the version of the ```Visual Studio C++ Compiler```
that is installed.

Then run:

    PUSHD src\main\c\Windows
    %VC_DIRECTORY%\vcvarsall.bat x64
    nmake win64
    %VC_DIRECTORY%\vcvarsall.bat x86
    nmake win32
    POPD
    gradle build

The resulting ```jSerialComm``` library can be found in the project directory
```build/libs``` under the name ```jSerialComm-{VERSION}.jar```
