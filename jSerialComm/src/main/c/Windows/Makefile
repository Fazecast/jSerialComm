## Windows specific library variables
COMPILE			= cl
LINK			= link
CFLAGS			= /c /O2 /GF /GL /MT /EHsc /fp:precise /J /nologo /TP
LDFLAGS			= /DLL /LTCG /NOASSEMBLY /NOLOGO
INCLUDES		= /I"$(JDK_HOME)\include" /I"$(JDK_HOME)\include\win32"
LIBRARIES		= Advapi32.lib SetupAPI.lib
DELETE			= @del /q /f
RMDIR			= @rd /q /s
MKDIR			= @md
COPY			= @copy /y
MOVE			= @move /y
PRINT			= @echo
FULL_CLASS		= com.fazecast.jSerialComm.SerialPort
JAVAC			= "$(JDK_HOME)\bin\javac"
JFLAGS 			= -source 1.6 -target 1.6 -Xlint:-options
LIBRARY_NAME	= jSerialComm.dll
SOURCES			= SerialPort_Windows.c WindowsHelperFunctions.c
JAVA_SOURCE_DIR	= ..\..\..\..\src\main\java\com\fazecast\jSerialComm
RESOURCE_DIR	= ..\..\..\..\src\main\resources\Windows
BUILD_DIR		= ..\..\..\..\bin\Windows
JAVA_CLASS_DIR	= $(BUILD_DIR)\..\com\fazecast\jSerialComm
OBJECTSx86		= $(BUILD_DIR)\x86\SerialPort_Windows.obj $(BUILD_DIR)\x86\WindowsHelperFunctions.obj
OBJECTSx86_64	= $(BUILD_DIR)\x86_64\SerialPort_Windows.obj $(BUILD_DIR)\x86_64\WindowsHelperFunctions.obj
JNI_HEADER		= ..\com_fazecast_jSerialComm_SerialPort.h
JAVA_CLASS		= $(JAVA_CLASS_DIR)\SerialPort.class

# Define phony and suffix rules
.PHONY: all win32 win64 checkdirs clean
.SUFFIXES:
.SUFFIXES: .c .obj .class .java .h

# Default build target not possible due to different architecture compilers
all :
	$(PRINT).
	$(PRINT) Must specify a target (either win32 or win64), but not both at the same time since different versions of the Microsoft Compiler are required for difference architectures.
	$(PRINT).
	$(PRINT) NOTE: Before attempting to use this Makefile, make sure that you have called 'vcvarsall.bat' for your intended architecture. This file can normally be found in "C:\Program Files (x86)\Microsoft Visual Studio [version]\VC\".
	$(PRINT).
	$(PRINT) Example: To build 64-bit Windows libraries, you would call:
	$(PRINT)     C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat x64
	$(PRINT)     nmake win64
	$(PRINT).

# Builds 32-bit Windows libraries
win32 : $(BUILD_DIR)\x86 $(BUILD_DIR)\x86\$(LIBRARY_NAME)
	$(COPY) $(BUILD_DIR)\x86\*.dll $(RESOURCE_DIR)\x86
	$(DELETE) ..\*.h
	$(RMDIR) $(BUILD_DIR)\..

# Builds 64-bit Windows libraries
win64 : $(BUILD_DIR)\x86_64 $(BUILD_DIR)\x86_64\$(LIBRARY_NAME)
	$(COPY) $(BUILD_DIR)\x86_64\*.dll $(RESOURCE_DIR)\x86_64
	$(DELETE) ..\*.h
	$(RMDIR) $(BUILD_DIR)\..

# Rule to create build directories
$(BUILD_DIR)\x86 :
	$(MKDIR) "$@"
$(BUILD_DIR)\x86_64 :
	$(MKDIR) "$@"

# Rule to build 32-bit library
$(BUILD_DIR)\x86\$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSx86)
	$(LINK) $(LDFLAGS) /MACHINE:X86 /OUT:$@ $(OBJECTSx86) $(LIBRARIES)

# Rule to build 64-bit library
$(BUILD_DIR)\x86_64\$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSx86_64)
	$(LINK) $(LDFLAGS) /MACHINE:X64 /OUT:$@ $(OBJECTSx86_64) $(LIBRARIES)

# Suffix rules to get from *.c -> *.obj
$(OBJECTSx86_64) :
	$(COMPILE) $(CFLAGS) $(INCLUDES) $(*B).c -Fo$@
$(OBJECTSx86) :
	$(COMPILE) $(CFLAGS) $(INCLUDES) $(*B).c -Fo$@

# Rule to build JNI header file
$(JNI_HEADER) : $(JAVA_CLASS)

# Suffix rule to get from *.java -> *.class
$(JAVA_CLASS) :
	$(JAVAC) $(JFLAGS) -d $(JAVA_CLASS_DIR)\..\..\.. -cp $(JAVA_SOURCE_DIR)\..\..\.. $(JAVA_SOURCE_DIR)\$(*B).java -h ..
