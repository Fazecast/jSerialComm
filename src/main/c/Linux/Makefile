# Linux specific library variables
COMPILE			:= gcc
COMPILE_ARM32	:= arm32-unknown-linux-gnueabi-gcc
COMPILE_ARM32HF	:= arm32hf-unknown-linux-gnueabi-gcc
COMPILE_ARM64	:= arm64-unknown-linux-gnueabi-gcc
LINK			:= gcc
LINK_ARM32		:= arm32-unknown-linux-gnueabi-gcc
LINK_ARM32HF	:= arm32hf-unknown-linux-gnueabi-gcc
LINK_ARM64		:= arm64-unknown-linux-gnueabi-gcc
CFLAGS			:= -fPIC -static-libgcc -Os -flto -fuse-linker-plugin
LDFLAGS			:= -shared -static-libgcc -flto -fuse-linker-plugin -Os
INCLUDES		:= -I"$(JDK_HOME)/include" -I"$(JDK_HOME)/include/linux"
LIBRARIES		:= 
DELETE			:= @rm
MKDIR			:= @mkdir
COPY			:= @cp
MOVE			:= @mv
PRINT			:= @echo
FULL_CLASS		:= com.fazecast.jSerialComm.SerialPort
JAVAC			:= "$(JDK_HOME)/bin/javac"
JAVAH			:= "$(JDK_HOME)/bin/javah" -jni
JFLAGS 			:= -source 1.6 -target 1.6 -Xlint:-options
LIBRARY_NAME	:= libjSerialComm.so
JAVA_SOURCE_DIR	= ../../../../src/main/java/com/fazecast/jSerialComm
RESOURCE_DIR	= ../../../../src/main/resources/Linux
BUILD_DIR		= ../../../../bin/Linux
JAVA_CLASS_DIR	= $(BUILD_DIR)/../com/fazecast/jSerialComm
OBJECTSx86		= $(BUILD_DIR)/x86/SerialPort_Linux.o $(BUILD_DIR)/x86/LinuxHelperFunctions.o
OBJECTSx86_64	= $(BUILD_DIR)/x86_64/SerialPort_Linux.o $(BUILD_DIR)/x86_64/LinuxHelperFunctions.o
OBJECTSarmv5	= $(BUILD_DIR)/armv5/SerialPort_Linux.o $(BUILD_DIR)/armv5/LinuxHelperFunctions.o
OBJECTSarmv6	= $(BUILD_DIR)/armv6/SerialPort_Linux.o $(BUILD_DIR)/armv6/LinuxHelperFunctions.o
OBJECTSarmv6-hf	= $(BUILD_DIR)/armv6-hf/SerialPort_Linux.o $(BUILD_DIR)/armv6-hf/LinuxHelperFunctions.o
OBJECTSarmv7	= $(BUILD_DIR)/armv7/SerialPort_Linux.o $(BUILD_DIR)/armv7/LinuxHelperFunctions.o
OBJECTSarmv7-hf	= $(BUILD_DIR)/armv7-hf/SerialPort_Linux.o $(BUILD_DIR)/armv7-hf/LinuxHelperFunctions.o
OBJECTSarmv8	= $(BUILD_DIR)/armv8/SerialPort_Linux.o $(BUILD_DIR)/armv8/LinuxHelperFunctions.o
OBJECTSarmv8_64	= $(BUILD_DIR)/armv8_64/SerialPort_Linux.o $(BUILD_DIR)/armv8_64/LinuxHelperFunctions.o
FLAGSarmv5		= -marm -mthumb-interwork -march=armv5 -mfloat-abi=soft
FLAGSarmv6		= -marm -mthumb-interwork -march=armv6 -mfloat-abi=soft
FLAGSarmv6hf	= -marm -mthumb-interwork -march=armv6 -mfloat-abi=hard
FLAGSarmv7		= -mthumb -mthumb-interwork -march=armv7 -mfloat-abi=soft
FLAGSarmv7hf	= -mthumb -mthumb-interwork -march=armv7 -mfloat-abi=hard
FLAGSarmv8		= -mthumb -mthumb-interwork -march=armv8-a -mfloat-abi=hard
FLAGSarmv8_64	= -mthumb -mthumb-interwork -march=armv8-a -mfloat-abi=hard
JNI_HEADER		= ../com_fazecast_jSerialComm_SerialPort.h
JAVA_CLASS		= $(JAVA_CLASS_DIR)/SerialPort.class

# Define phony and suffix rules
.PHONY: all checkdirs linux32 linux64 arm armv5 armv6 armv6-hf armv7 armv7-hf armv8 armv8_64
.SUFFIXES:
.SUFFIXES: .cpp .c .o .class .java .h

# Default build target builds both 32 and 64 bit libraries
all : linux32 linux64
	$(DELETE) -rf `readlink -f $(BUILD_DIR)/..`
	$(DELETE) -rf ../*.h
	
# Builds 32-bit Linux libraries
linux32 : ARCH = -m32
linux32 : checkdirs $(BUILD_DIR)/x86/$(LIBRARY_NAME)
	$(DELETE) -rf $(BUILD_DIR)/x86/*.o
	$(COPY) $(BUILD_DIR)/x86/*.* $(RESOURCE_DIR)/x86/

# Builds 64-bit Linux libraries
linux64 : ARCH = -m64
linux64 : checkdirs $(BUILD_DIR)/x86_64/$(LIBRARY_NAME)
	$(DELETE) -rf $(BUILD_DIR)/x86_64/*.o
	$(COPY) $(BUILD_DIR)/x86_64/*.* $(RESOURCE_DIR)/x86_64/

# Build all ARM libraries
arm : armv5 armv6 armv6-hf armv7 armv7-hf #armv8 armv8_64
	$(DELETE) -rf `readlink -f $(BUILD_DIR)/..`
	$(DELETE) -rf ../*.h
armv5 : checkdirs $(BUILD_DIR)/armv5/$(LIBRARY_NAME)
	$(DELETE) -rf $(BUILD_DIR)/armv5/*.o
	$(COPY) $(BUILD_DIR)/armv5/*.* $(RESOURCE_DIR)/armv5/
armv6 : checkdirs $(BUILD_DIR)/armv6/$(LIBRARY_NAME)
	$(DELETE) -rf $(BUILD_DIR)/armv6/*.o
	$(COPY) $(BUILD_DIR)/armv6/*.* $(RESOURCE_DIR)/armv6/
armv6-hf : checkdirs $(BUILD_DIR)/armv6-hf/$(LIBRARY_NAME)
	$(DELETE) -rf $(BUILD_DIR)/armv6-hf/*.o
	$(COPY) $(BUILD_DIR)/armv6-hf/*.* $(RESOURCE_DIR)/armv6-hf/
armv7 : checkdirs $(BUILD_DIR)/armv7/$(LIBRARY_NAME)
	$(DELETE) -rf $(BUILD_DIR)/armv7/*.o
	$(COPY) $(BUILD_DIR)/armv7/*.* $(RESOURCE_DIR)/armv7/
armv7-hf : checkdirs $(BUILD_DIR)/armv7-hf/$(LIBRARY_NAME)
	$(DELETE) -rf $(BUILD_DIR)/armv7-hf/*.o
	$(COPY) $(BUILD_DIR)/armv7-hf/*.* $(RESOURCE_DIR)/armv7-hf/
armv8 : checkdirs $(BUILD_DIR)/armv8/$(LIBRARY_NAME)
	$(DELETE) -rf $(BUILD_DIR)/armv8/*.o
	$(COPY) $(BUILD_DIR)/armv8/*.* $(RESOURCE_DIR)/armv8/
armv8_64 : checkdirs $(BUILD_DIR)/armv8_64/$(LIBRARY_NAME)
	$(DELETE) -rf $(BUILD_DIR)/armv8_64/*.o
	$(COPY) $(BUILD_DIR)/armv8_64/*.* $(RESOURCE_DIR)/armv8_64/

# Rule to create build directories
checkdirs : $(BUILD_DIR)/x86 $(BUILD_DIR)/x86_64 $(BUILD_DIR)/armv5 $(BUILD_DIR)/armv6 $(BUILD_DIR)/armv6-hf $(BUILD_DIR)/armv7 $(BUILD_DIR)/armv7-hf $(BUILD_DIR)/armv8 $(BUILD_DIR)/armv8_64 $(JAVA_CLASS_DIR)
$(BUILD_DIR)/x86 :
	$(MKDIR) -p $@
$(BUILD_DIR)/x86_64 :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv5 :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv6 :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv6-hf :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv7 :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv7-hf :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv8 :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv8_64 :
	$(MKDIR) -p $@
$(JAVA_CLASS_DIR) :
	$(MKDIR) -p $@

# Rule to build 32-bit library
$(BUILD_DIR)/x86/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSx86)
	$(LINK) $(LDFLAGS) $(ARCH) -o $@ $(OBJECTSx86) $(LIBRARIES)

# Rule to build 64-bit library
$(BUILD_DIR)/x86_64/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSx86_64)
	$(LINK) $(LDFLAGS) $(ARCH) -o $@ $(OBJECTSx86_64) $(LIBRARIES)

# Rule to build ARM libraries
$(BUILD_DIR)/armv5/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSarmv5)
	$(LINK_ARM32) $(LDFLAGS) $(FLAGSarmv5) -o $@ $(OBJECTSarmv5) $(LIBRARIES)
$(BUILD_DIR)/armv6/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSarmv6)
	$(LINK_ARM32) $(LDFLAGS) $(FLAGSarmv6) -o $@ $(OBJECTSarmv6) $(LIBRARIES)
$(BUILD_DIR)/armv6-hf/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSarmv6-hf)
	$(LINK_ARM32HF) $(LDFLAGS) $(FLAGSarmv6-hf) -o $@ $(OBJECTSarmv6-hf) $(LIBRARIES)
$(BUILD_DIR)/armv7/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSarmv7)
	$(LINK_ARM32) $(LDFLAGS) $(FLAGSarmv7) -o $@ $(OBJECTSarmv7) $(LIBRARIES)
$(BUILD_DIR)/armv7-hf/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSarmv7-hf)
	$(LINK_ARM32HF) $(LDFLAGS) $(FLAGSarmv7-hf) -o $@ $(OBJECTSarmv7-hf) $(LIBRARIES)
$(BUILD_DIR)/armv8/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSarmv8)
	$(LINK_ARM32) $(LDFLAGS) $(FLAGSarmv8) -o $@ $(OBJECTSarmv8) $(LIBRARIES)
$(BUILD_DIR)/armv8_64/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSarmv8_64)
	$(LINK_ARM64) $(LDFLAGS) $(FLAGSarmv8_64) -o $@ $(OBJECTSarmv8_64) $(LIBRARIES)
	
# Suffix rules to get from *.c -> *.o
$(BUILD_DIR)/x86/%.o : %.c
	$(COMPILE) $(INCLUDES) $(CFLAGS) $(ARCH) -c $< -o $@
$(BUILD_DIR)/x86_64/%.o : %.c
	$(COMPILE) $(INCLUDES) $(CFLAGS) $(ARCH) -c $< -o $@
$(BUILD_DIR)/armv5/%.o : %.c
	$(COMPILE_ARM32) $(INCLUDES) $(CFLAGS) $(FLAGSarmv5) -c $< -o $@
$(BUILD_DIR)/armv6/%.o : %.c
	$(COMPILE_ARM32) $(INCLUDES) $(CFLAGS) $(FLAGSarmv6) -c $< -o $@
$(BUILD_DIR)/armv6-hf/%.o : %.c
	$(COMPILE_ARM32HF) $(INCLUDES) $(CFLAGS) $(FLAGSarmv6-hf) -c $< -o $@
$(BUILD_DIR)/armv7/%.o : %.c
	$(COMPILE_ARM32) $(INCLUDES) $(CFLAGS) $(FLAGSarmv7) -c $< -o $@
$(BUILD_DIR)/armv7-hf/%.o : %.c
	$(COMPILE_ARM32HF) $(INCLUDES) $(CFLAGS) $(FLAGSarmv7-hf) -c $< -o $@
$(BUILD_DIR)/armv8/%.o : %.c
	$(COMPILE_ARM32) $(INCLUDES) $(CFLAGS) $(FLAGSarmv8) -c $< -o $@
$(BUILD_DIR)/armv8_64/%.o : %.c
	$(COMPILE_ARM64) $(INCLUDES) $(CFLAGS) $(FLAGSarmv8_64) -c $< -o $@

# Rule to build JNI header file
$(JNI_HEADER) : $(JAVA_CLASS)
	$(JAVAH) -d .. -classpath $(JAVA_CLASS_DIR)/../../.. $(FULL_CLASS)

# Suffix rule to get from *.java -> *.class
$(JAVA_CLASS) :
	$(JAVAC) $(JFLAGS) -d $(JAVA_CLASS_DIR)/../../.. -cp $(JAVA_SOURCE_DIR)/../../.. $(JAVA_SOURCE_DIR)/$(basename $(notdir $@)).java
