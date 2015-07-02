# OSX specific library variables
COMPILE			:= gcc
LINK			:= gcc
CFLAGS			:=
LDFLAGS			:= -dynamiclib
JDK_HOME		= `/usr/libexec/java_home`
INCLUDES		:= -I"$(JDK_HOME)/include" -I"$(JDK_HOME)/include/darwin"
LIBRARIES		:= -framework Cocoa -framework IOKit -framework JavaVM
DELETE			:= @rm
MKDIR			:= @mkdir
COPY			:= @cp
MOVE			:= @mv
PRINT			:= @echo
FULL_CLASS		:= com.fazecast.jSerialComm.SerialPort
JAVAC			:= javac
JAVAH			:= javah -jni
JFLAGS 			:= -source 1.6 -target 1.6 -Xlint:-options
LIBRARY_NAME	:= libjSerialComm.jnilib
JAVA_SOURCE_DIR	= ../../../../src/main/java/com/fazecast/jSerialComm
RESOURCE_DIR	= ../../../../src/main/resources/OSX
BUILD_DIR		= ../../../../bin/OSX
JAVA_CLASS_DIR	= $(BUILD_DIR)/../com/fazecast/jSerialComm
OBJECTSx86		= $(BUILD_DIR)/x86/SerialPort_OSX.o $(BUILD_DIR)/x86/OSXHelperFunctions.o
OBJECTSx86_64	= $(BUILD_DIR)/x86_64/SerialPort_OSX.o $(BUILD_DIR)/x86_64/OSXHelperFunctions.o
JNI_HEADER		= ../com_fazecast_jSerialComm_SerialPort.h
JAVA_CLASS		= $(JAVA_CLASS_DIR)/SerialPort.class

# Define phony and suffix rules
.PHONY: all osx32 osx64 checkdirs
.SUFFIXES:
.SUFFIXES: .cpp .c .o .class .java .h

# Default build target builds both 32 and 64 bit libraries
all : osx32 osx64
	$(DELETE) -rf $(BUILD_DIR)
	$(DELETE) -rf ../*.h

# Builds 32-bit OSX libraries
osx32 : ARCH = -m32
osx32 : checkdirs $(BUILD_DIR)/x86/$(LIBRARY_NAME)
	$(DELETE) -rf $(BUILD_DIR)/x86/*.o
	$(COPY) $(BUILD_DIR)/x86/*.* $(RESOURCE_DIR)/x86

# Builds 64-bit OSX libraries
osx64 : ARCH = -m64
osx64 : checkdirs $(BUILD_DIR)/x86_64/$(LIBRARY_NAME)
	$(DELETE) -rf $(BUILD_DIR)/x86_64/*.o
	$(COPY) $(BUILD_DIR)/x86_64/*.* $(RESOURCE_DIR)/x86_64

# Rule to create build directories
checkdirs : $(BUILD_DIR)/x86 $(BUILD_DIR)/x86_64 $(JAVA_CLASS_DIR)
$(BUILD_DIR)/x86 :
	$(MKDIR) -p $@
$(BUILD_DIR)/x86_64 :
	$(MKDIR) -p $@
$(JAVA_CLASS_DIR) :
	$(MKDIR) -p $@

# Rule to build 32-bit library
$(BUILD_DIR)/x86/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSx86)
	$(LINK) $(LDFLAGS) $(ARCH) -o $@ $(OBJECTSx86) $(LIBRARIES)

# Rule to build 64-bit library
$(BUILD_DIR)/x86_64/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSx86_64)
	$(LINK) $(LDFLAGS) $(ARCH) -o $@ $(OBJECTSx86_64) $(LIBRARIES)
	
# Suffix rules to get from *.c -> *.o
$(BUILD_DIR)/x86/%.o : %.c
	$(COMPILE) $(INCLUDES) $(CFLAGS) $(ARCH) -c $< -o $@
$(BUILD_DIR)/x86_64/%.o : %.c
	$(COMPILE) $(INCLUDES) $(CFLAGS) $(ARCH) -c $< -o $@

# Rule to build JNI header file
$(JNI_HEADER) : $(JAVA_CLASS)
	$(JAVAH) -d .. -classpath $(JAVA_CLASS_DIR)/../../.. $(FULL_CLASS)

# Suffix rule to get from *.java -> *.class
$(JAVA_CLASS) :
	$(JAVAC) $(JFLAGS) -d $(JAVA_CLASS_DIR)/../../.. -cp $(JAVA_SOURCE_DIR)/../../.. $(JAVA_SOURCE_DIR)/$(basename $(notdir $@)).java
