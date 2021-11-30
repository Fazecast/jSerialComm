# Compiler tools, commands, and flags
COMPILE_LINUX_X86           := i486-unknown-linux-gnu-gcc
COMPILE_LINUX_X86_64        := x86_64-unknown-linux-gnu-gcc
COMPILE_ARM32               := arm-unknown-linux-gnueabi-gcc
COMPILE_ARM32HF             := arm-unknown-linux-gnueabihf-gcc
COMPILE_ARM64               := aarch64-unknown-linux-gnu-gcc
COMPILE_PPC64LE             := powerpc64le-unknown-linux-gnu-gcc
COMPILE_SOLARIS_X86         := x86_64-sun-solaris2.10-gcc
COMPILE_SOLARIS_SPARC       := sparc-sun-solaris2.10-gcc
COMPILE_OSX_X86             := o32-clang
COMPILE_OSX_X86_64          := o64-clang
COMPILE_OSX_ARM64           := oa64-clang
COMPILE_ANDROID             := ndk-build
CFLAGS_POSIX                := -fPIC -Os -flto -static-libgcc -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fuse-linker-plugin
CFLAGS_APPLE                := -fPIC -Os -flto -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
LDFLAGS_POSIX               := -Os -flto -static-libgcc -shared -fuse-linker-plugin -s
LDFLAGS_APPLE               := -Os -dynamiclib -flto -shared -fuse-linker-plugin
JDK_HOME                    := $(shell if [ "`uname`" = "Darwin" ]; then echo "`/usr/libexec/java_home`"; else echo "$$JDK_HOME"; fi)
INCLUDES                    := -I"$(JDK_HOME)/include" -I"$(JDK_HOME)/include/linux" -I"$(JDK_HOME)/include/darwin" -I"$(JDK_HOME)/include/solaris"
LIBRARIES_POSIX             := 
LIBRARIES_APPLE             := -framework Cocoa -framework IOKit
DELETE                      := @rm
MKDIR                       := @mkdir
COPY                        := @cp
MOVE                        := @mv
PRINT                       := @echo

# Java and resource definitions
LIBRARY_NAME_POSIX          := libjSerialComm.so
LIBRARY_NAME_APPLE          := libjSerialComm.jnilib
BUILD_DIR                   := ../../../../build
RESOURCE_DIR_LINUX          := ../../../../src/main/resources/Linux
RESOURCE_DIR_SOLARIS        := ../../../../src/main/resources/Solaris
RESOURCE_DIR_OSX            := ../../../../src/main/resources/OSX
JAVA_SOURCE_DIR             := ../../../../src/main/java/com/fazecast/jSerialComm
FULL_CLASS                  := com.fazecast.jSerialComm.SerialPort
JAVA_CLASS_DIR              := $(BUILD_DIR)/com/fazecast/jSerialComm
JNI_HEADER                  := ../com_fazecast_jSerialComm_SerialPort.h
JAVA_CLASS                  := $(JAVA_CLASS_DIR)/SerialPort.class
JFLAGS                      := -source 1.6 -target 1.6 -Xlint:-options
JAVAC                       := "$(JDK_HOME)/bin/javac"

# Architecture-dependent library variables
OBJECTSlinux32              := $(BUILD_DIR)/linux_x86/SerialPort_Posix.o $(BUILD_DIR)/linux_x86/PosixHelperFunctions.o
OBJECTSlinux64              := $(BUILD_DIR)/linux_x86_64/SerialPort_Posix.o $(BUILD_DIR)/linux_x86_64/PosixHelperFunctions.o
OBJECTSarmv5                := $(BUILD_DIR)/armv5/SerialPort_Posix.o $(BUILD_DIR)/armv5/PosixHelperFunctions.o
OBJECTSarmv6                := $(BUILD_DIR)/armv6/SerialPort_Posix.o $(BUILD_DIR)/armv6/PosixHelperFunctions.o
OBJECTSarmv6hf              := $(BUILD_DIR)/armv6hf/SerialPort_Posix.o $(BUILD_DIR)/armv6hf/PosixHelperFunctions.o
OBJECTSarmv7                := $(BUILD_DIR)/armv7/SerialPort_Posix.o $(BUILD_DIR)/armv7/PosixHelperFunctions.o
OBJECTSarmv7hf              := $(BUILD_DIR)/armv7hf/SerialPort_Posix.o $(BUILD_DIR)/armv7hf/PosixHelperFunctions.o
OBJECTSarmv8_32             := $(BUILD_DIR)/armv8_32/SerialPort_Posix.o $(BUILD_DIR)/armv8_32/PosixHelperFunctions.o
OBJECTSarmv8_64             := $(BUILD_DIR)/armv8_64/SerialPort_Posix.o $(BUILD_DIR)/armv8_64/PosixHelperFunctions.o
OBJECTSppc64le              := $(BUILD_DIR)/ppc64le/SerialPort_Posix.o $(BUILD_DIR)/ppc64le/PosixHelperFunctions.o
OBJECTSsolaris32            := $(BUILD_DIR)/solaris_x86/SerialPort_Posix.o $(BUILD_DIR)/solaris_x86/PosixHelperFunctions.o
OBJECTSsolaris64            := $(BUILD_DIR)/solaris_x86_64/SerialPort_Posix.o $(BUILD_DIR)/solaris_x86_64/PosixHelperFunctions.o
OBJECTSsolarisSparc32       := $(BUILD_DIR)/solaris_sparc32/SerialPort_Posix.o $(BUILD_DIR)/solaris_sparc32/PosixHelperFunctions.o
OBJECTSsolarisSparc64       := $(BUILD_DIR)/solaris_sparc64/SerialPort_Posix.o $(BUILD_DIR)/solaris_sparc64/PosixHelperFunctions.o
OBJECTSosx86                := $(BUILD_DIR)/osx_x86/SerialPort_Posix.o $(BUILD_DIR)/osx_x86/PosixHelperFunctions.o
OBJECTSosx64                := $(BUILD_DIR)/osx_x86_64/SerialPort_Posix.o $(BUILD_DIR)/osx_x86_64/PosixHelperFunctions.o
OBJECTSosxaarch64           := $(BUILD_DIR)/osx_aarch64/SerialPort_Posix.o $(BUILD_DIR)/osx_aarch64/PosixHelperFunctions.o
FLAGSlinux32                := -m32
FLAGSlinux64                := -m64
FLAGSarmv5                  := -marm -mthumb-interwork -march=armv5 -mfloat-abi=soft
FLAGSarmv6                  := -marm -mthumb-interwork -march=armv6 -mfloat-abi=soft
FLAGSarmv6hf                := -marm -mthumb-interwork -march=armv6+fp -mfloat-abi=hard
FLAGSarmv7                  := -mthumb -mthumb-interwork -march=armv7 -mfloat-abi=soft
FLAGSarmv7hf                := -mthumb -mthumb-interwork -march=armv7+fp -mfloat-abi=hard
FLAGSarmv8_32               := -mthumb -mthumb-interwork -march=armv8-a
FLAGSarmv8_64               := -march=armv8-a
FLAGSppc64le                := -mtune=powerpc64le -m64
FLAGSsolaris32              := -m32
FLAGSsolaris64              := -m64
FLAGSsolarisSparc32         := -mno-app-regs -mcpu=v8 -mv8plus -m32
FLAGSsolarisSparc64         := -mno-app-regs -mcpu=v9 -m64
FLAGSosx86                  := -arch i386 -m32
FLAGSosx64                  := -arch x86_64 -m64
FLAGSosxaarch64             := -arch arm64

# Define phony and suffix rules
.PHONY: all clean linux linux32 linux64 arm armv5 armv6 armv6hf armv7 armv7hf armv8_32 armv8_64 powerpc ppc64le solaris solaris32 solaris64 solarisSparc32 solarisSparc64 osx osx32 osx64 osxaarch64 android
.SUFFIXES:
.SUFFIXES: .cpp .c .o .class .java .h

# Default build target does nothing
all :
	$(PRINT) You must specify either linux, arm, solaris, osx, powerpc, or android!
clean :
	$(DELETE) -rf "$(BUILD_DIR)"
	$(DELETE) -rf ../*.h

# Build all Linux libraries
linux : linux32 linux64
	$(DELETE) -rf "`readlink -f $(BUILD_DIR)`"
	$(DELETE) -rf ../*.h

# Build all ARM libraries
arm : armv5 armv6 armv6hf armv7 armv7hf armv8_32 armv8_64
	$(DELETE) -rf "`readlink -f $(BUILD_DIR)`"
	$(DELETE) -rf ../*.h

# Build all PowerPC libraries
powerpc : ppc64le
	$(DELETE) -rf "`readlink -f $(BUILD_DIR)`"
	$(DELETE) -rf ../*.h

# Build all Solaris libraries
solaris : solaris32 solaris64 solarisSparc32 solarisSparc64
	$(DELETE) -rf "`readlink -f $(BUILD_DIR)`"
	$(DELETE) -rf ../*.h

# Build all OSX libraries
osx : osx32 osx64 osxaarch64
	$(DELETE) -rf "$(BUILD_DIR)"
	$(DELETE) -rf ../*.h

# Build architecture-specific Linux libraries
linux32 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/linux_x86 $(BUILD_DIR)/linux_x86/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/linux_x86/*.o
	$(COPY) $(BUILD_DIR)/linux_x86/*.* $(RESOURCE_DIR_LINUX)/x86/
linux64 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/linux_x86_64 $(BUILD_DIR)/linux_x86_64/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/linux_x86_64/*.o
	$(COPY) $(BUILD_DIR)/linux_x86_64/*.* $(RESOURCE_DIR_LINUX)/x86_64/

# Build architecture-specific ARM libraries
armv5 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/armv5 $(BUILD_DIR)/armv5/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/armv5/*.o
	$(COPY) $(BUILD_DIR)/armv5/*.* $(RESOURCE_DIR_LINUX)/armv5/
armv6 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/armv6 $(BUILD_DIR)/armv6/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/armv6/*.o
	$(COPY) $(BUILD_DIR)/armv6/*.* $(RESOURCE_DIR_LINUX)/armv6/
armv6hf : $(JAVA_CLASS_DIR) $(BUILD_DIR)/armv6hf $(BUILD_DIR)/armv6hf/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/armv6hf/*.o
	$(COPY) $(BUILD_DIR)/armv6hf/*.* $(RESOURCE_DIR_LINUX)/armv6hf/
armv7 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/armv7 $(BUILD_DIR)/armv7/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/armv7/*.o
	$(COPY) $(BUILD_DIR)/armv7/*.* $(RESOURCE_DIR_LINUX)/armv7/
armv7hf : $(JAVA_CLASS_DIR) $(BUILD_DIR)/armv7hf $(BUILD_DIR)/armv7hf/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/armv7hf/*.o
	$(COPY) $(BUILD_DIR)/armv7hf/*.* $(RESOURCE_DIR_LINUX)/armv7hf/
armv8_32 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/armv8_32 $(BUILD_DIR)/armv8_32/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/armv8_32/*.o
	$(COPY) $(BUILD_DIR)/armv8_32/*.* $(RESOURCE_DIR_LINUX)/armv8_32/
armv8_64 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/armv8_64 $(BUILD_DIR)/armv8_64/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/armv8_64/*.o
	$(COPY) $(BUILD_DIR)/armv8_64/*.* $(RESOURCE_DIR_LINUX)/armv8_64/

# Build architecture-specific POWERPC libraries
ppc64le : $(JAVA_CLASS_DIR) $(BUILD_DIR)/ppc64le $(BUILD_DIR)/ppc64le/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/ppc64le/*.o
	$(COPY) $(BUILD_DIR)/ppc64le/*.* $(RESOURCE_DIR_LINUX)/ppc64le/

# Build architecture-specific Solaris libraries
solaris32 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/solaris_x86 $(BUILD_DIR)/solaris_x86/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/solaris_x86/*.o
	$(COPY) $(BUILD_DIR)/solaris_x86/*.* $(RESOURCE_DIR_SOLARIS)/x86/
solaris64 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/solaris_x86_64 $(BUILD_DIR)/solaris_x86_64/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/solaris_x86_64/*.o
	$(COPY) $(BUILD_DIR)/solaris_x86_64/*.* $(RESOURCE_DIR_SOLARIS)/x86_64/
solarisSparc32 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/solaris_sparc32 $(BUILD_DIR)/solaris_sparc32/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/solaris_sparc32/*.o
	$(COPY) $(BUILD_DIR)/solaris_sparc32/*.* $(RESOURCE_DIR_SOLARIS)/sparcv8plus_32/
solarisSparc64 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/solaris_sparc64 $(BUILD_DIR)/solaris_sparc64/$(LIBRARY_NAME_POSIX)
	$(DELETE) -rf $(BUILD_DIR)/solaris_sparc64/*.o
	$(COPY) $(BUILD_DIR)/solaris_sparc64/*.* $(RESOURCE_DIR_SOLARIS)/sparcv9_64/

# Build architecture-specific OSX libraries
osx32 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/osx_x86 $(BUILD_DIR)/osx_x86/$(LIBRARY_NAME_APPLE)
	$(DELETE) -rf $(BUILD_DIR)/osx_x86/*.o
	$(COPY) $(BUILD_DIR)/osx_x86/*.* $(RESOURCE_DIR_OSX)/x86/
osx64 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/osx_x86_64 $(BUILD_DIR)/osx_x86_64/$(LIBRARY_NAME_APPLE)
	$(DELETE) -rf $(BUILD_DIR)/osx_x86_64/*.o
	$(COPY) $(BUILD_DIR)/osx_x86_64/*.* $(RESOURCE_DIR_OSX)/x86_64/
osxaarch64 : $(JAVA_CLASS_DIR) $(BUILD_DIR)/osx_aarch64 $(BUILD_DIR)/osx_aarch64/$(LIBRARY_NAME_APPLE)
	$(DELETE) -rf $(BUILD_DIR)/osx_aarch64/*.o
	$(COPY) $(BUILD_DIR)/osx_aarch64/*.* $(RESOURCE_DIR_OSX)/aarch64/

# Build Android libraries
android :
	$(COMPILE_ANDROID)

# Rule to create build directories
$(BUILD_DIR)/linux_x86 :
	$(MKDIR) -p $@
$(BUILD_DIR)/linux_x86_64 :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv5 :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv6 :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv6hf :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv7 :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv7hf :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv8_32 :
	$(MKDIR) -p $@
$(BUILD_DIR)/armv8_64 :
	$(MKDIR) -p $@
$(BUILD_DIR)/ppc64le :
	$(MKDIR) -p $@
$(BUILD_DIR)/solaris_x86 :
	$(MKDIR) -p $@
$(BUILD_DIR)/solaris_x86_64 :
	$(MKDIR) -p $@
$(BUILD_DIR)/solaris_sparc32 :
	$(MKDIR) -p $@
$(BUILD_DIR)/solaris_sparc64 :
	$(MKDIR) -p $@
$(BUILD_DIR)/osx_x86 :
	$(MKDIR) -p $@
$(BUILD_DIR)/osx_x86_64 :
	$(MKDIR) -p $@
$(BUILD_DIR)/osx_aarch64 :
	$(MKDIR) -p $@
$(JAVA_CLASS_DIR) :
	$(MKDIR) -p $@

# Build rules for all libraries
$(BUILD_DIR)/linux_x86/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSlinux32)
	$(COMPILE_LINUX_X86) $(LDFLAGS_POSIX) $(FLAGSlinux32) -o $@ $(OBJECTSlinux32) $(LIBRARIES_POSIX)
$(BUILD_DIR)/linux_x86_64/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSlinux64)
	$(COMPILE_LINUX_X86_64) $(LDFLAGS_POSIX) $(FLAGSlinux64) -o $@ $(OBJECTSlinux64) $(LIBRARIES_POSIX)
$(BUILD_DIR)/armv5/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSarmv5)
	$(COMPILE_ARM32) $(LDFLAGS_POSIX) $(FLAGSarmv5) -o $@ $(OBJECTSarmv5) $(LIBRARIES_POSIX)
$(BUILD_DIR)/armv6/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSarmv6)
	$(COMPILE_ARM32) $(LDFLAGS_POSIX) $(FLAGSarmv6) -o $@ $(OBJECTSarmv6) $(LIBRARIES_POSIX)
$(BUILD_DIR)/armv6hf/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSarmv6hf)
	$(COMPILE_ARM32HF) $(LDFLAGS_POSIX) $(FLAGSarmv6hf) -o $@ $(OBJECTSarmv6hf) $(LIBRARIES_POSIX)
$(BUILD_DIR)/armv7/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSarmv7)
	$(COMPILE_ARM32) $(LDFLAGS_POSIX) $(FLAGSarmv7) -o $@ $(OBJECTSarmv7) $(LIBRARIES_POSIX)
$(BUILD_DIR)/armv7hf/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSarmv7hf)
	$(COMPILE_ARM32HF) $(LDFLAGS_POSIX) $(FLAGSarmv7hf) -o $@ $(OBJECTSarmv7hf) $(LIBRARIES_POSIX)
$(BUILD_DIR)/armv8_32/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSarmv8_32)
	$(COMPILE_ARM32) $(LDFLAGS_POSIX) $(FLAGSarmv8_32) -o $@ $(OBJECTSarmv8_32) $(LIBRARIES_POSIX)
$(BUILD_DIR)/armv8_64/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSarmv8_64)
	$(COMPILE_ARM64) $(LDFLAGS_POSIX) $(FLAGSarmv8_64) -o $@ $(OBJECTSarmv8_64) $(LIBRARIES_POSIX)
$(BUILD_DIR)/ppc64le/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSppc64le)
	$(COMPILE_PPC64LE) $(LDFLAGS_POSIX) $(FLAGSppc64le) -o $@ $(OBJECTSppc64le) $(LIBRARIES_POSIX)
$(BUILD_DIR)/solaris_x86/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSsolaris32)
	$(COMPILE_SOLARIS_X86)  $(LDFLAGS_POSIX) $(FLAGSsolaris32) -o $@ $(OBJECTSsolaris32) $(LIBRARIES_POSIX)
$(BUILD_DIR)/solaris_x86_64/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSsolaris64)
	$(COMPILE_SOLARIS_X86) $(LDFLAGS_POSIX) $(FLAGSsolaris64) -o $@ $(OBJECTSsolaris64) $(LIBRARIES_POSIX)
$(BUILD_DIR)/solaris_sparc32/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSsolarisSparc32)
	$(COMPILE_SOLARIS_SPARC) $(LDFLAGS_POSIX) $(FLAGSsolarisSparc32) -o $@ $(OBJECTSsolarisSparc32) $(LIBRARIES_POSIX)
$(BUILD_DIR)/solaris_sparc64/$(LIBRARY_NAME_POSIX) : $(JNI_HEADER) $(OBJECTSsolarisSparc64)
	$(COMPILE_SOLARIS_SPARC) $(LDFLAGS_POSIX) $(FLAGSsolarisSparc64) -o $@ $(OBJECTSsolarisSparc64) $(LIBRARIES_POSIX)
$(BUILD_DIR)/osx_x86/$(LIBRARY_NAME_APPLE) : $(JNI_HEADER) $(OBJECTSosx86)
	$(COMPILE_OSX_X86) $(LDFLAGS_APPLE) $(FLAGSosx86) -o $@ $(OBJECTSosx86) $(LIBRARIES_APPLE)
$(BUILD_DIR)/osx_x86_64/$(LIBRARY_NAME_APPLE) : $(JNI_HEADER) $(OBJECTSosx64)
	$(COMPILE_OSX_X86_64) $(LDFLAGS_APPLE) $(FLAGSosx64) -o $@ $(OBJECTSosx64) $(LIBRARIES_APPLE)
$(BUILD_DIR)/osx_aarch64/$(LIBRARY_NAME_APPLE) : $(JNI_HEADER) $(OBJECTSosxaarch64)
	$(COMPILE_OSX_ARM64) $(LDFLAGS_APPLE) $(FLAGSosxaarch64) -o $@ $(OBJECTSosxaarch64) $(LIBRARIES_APPLE)

# Suffix rules to get from *.c -> *.o
$(BUILD_DIR)/linux_x86/%.o : %.c
	$(COMPILE_LINUX_X86) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSlinux32) -c $< -o $@
$(BUILD_DIR)/linux_x86_64/%.o : %.c
	$(COMPILE_LINUX_X86_64) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSlinux64) -c $< -o $@
$(BUILD_DIR)/armv5/%.o : %.c
	$(COMPILE_ARM32) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSarmv5) -c $< -o $@
$(BUILD_DIR)/armv6/%.o : %.c
	$(COMPILE_ARM32) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSarmv6) -c $< -o $@
$(BUILD_DIR)/armv6hf/%.o : %.c
	$(COMPILE_ARM32HF) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSarmv6hf) -c $< -o $@
$(BUILD_DIR)/armv7/%.o : %.c
	$(COMPILE_ARM32) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSarmv7) -c $< -o $@
$(BUILD_DIR)/armv7hf/%.o : %.c
	$(COMPILE_ARM32HF) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSarmv7hf) -c $< -o $@
$(BUILD_DIR)/armv8_32/%.o : %.c
	$(COMPILE_ARM32) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSarmv8_32) -c $< -o $@
$(BUILD_DIR)/armv8_64/%.o : %.c
	$(COMPILE_ARM64) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSarmv8_64) -c $< -o $@
$(BUILD_DIR)/ppc64le/%.o : %.c
	$(COMPILE_PPC64LE) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSppc64le) -c $< -o $@
$(BUILD_DIR)/solaris_x86/%.o : %.c
	$(COMPILE_SOLARIS_X86) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSsolaris32) -c $< -o $@
$(BUILD_DIR)/solaris_x86_64/%.o : %.c
	$(COMPILE_SOLARIS_X86) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSsolaris64) -c $< -o $@
$(BUILD_DIR)/solaris_sparc32/%.o : %.c
	$(COMPILE_SOLARIS_SPARC) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSsolarisSparc32) -c $< -o $@
$(BUILD_DIR)/solaris_sparc64/%.o : %.c
	$(COMPILE_SOLARIS_SPARC) $(INCLUDES) $(CFLAGS_POSIX) $(FLAGSsolarisSparc64) -c $< -o $@
$(BUILD_DIR)/osx_x86/%.o : %.c
	$(COMPILE_OSX_X86) $(INCLUDES) $(CFLAGS_APPLE) $(FLAGSosx86) -c $< -o $@
$(BUILD_DIR)/osx_x86_64/%.o : %.c
	$(COMPILE_OSX_X86_64) $(INCLUDES) $(CFLAGS_APPLE) $(FLAGSosx64) -c $< -o $@
$(BUILD_DIR)/osx_aarch64/%.o : %.c
	$(COMPILE_OSX_ARM64) $(INCLUDES) $(CFLAGS_APPLE) $(FLAGSosxaarch64) -c $< -o $@

# Rule to build JNI header file
$(JNI_HEADER) : $(JAVA_CLASS)

# Suffix rule to get from *.java -> *.class
$(JAVA_CLASS) :
	$(JAVAC) $(JFLAGS) -d $(JAVA_CLASS_DIR)/../../.. -cp $(JAVA_SOURCE_DIR)/../../.. $(JAVA_SOURCE_DIR)/$(basename $(notdir $@)).java -h ..
