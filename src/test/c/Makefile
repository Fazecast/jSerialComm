# Compiler tools, commands, and flags
COMPILE                     := gcc
COMPILE_WIN                 := x86_64-w64-mingw32-gcc
BUILD_DIR                   := build
JDK_HOME                    := $(shell if [ "`uname`" = "Darwin" ]; then echo "`/usr/libexec/java_home`"; else echo "$$JDK_HOME"; fi)
INCLUDES                    := -I"../../main/c/Posix" -I"../../main/c/Windows" -I"$(JDK_HOME)/include" -I"$(JDK_HOME)/include/linux" -I"$(JDK_HOME)/include/darwin" -I"$(JDK_HOME)/include/solaris"
CFLAGS                      := -fPIC -O0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 $(shell if [ "`uname`" != "Darwin" ]; then echo "-static-libgcc"; fi)
CFLAGS_WIN                  := -O0 -static-libgcc -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
LDFLAGS                     := -O0 $(shell if [ "`uname`" != "Darwin" ]; then echo "-static-libgcc"; fi)
LDFLAGS_WIN                 := -O0 -static-libgcc
LIBRARIES                   := $(shell if [ "`uname`" = "Darwin" ]; then echo "-framework Cocoa -framework IOKit"; else echo "-pthread"; fi)
LIBRARIES_WIN               := -ladvapi32 -lsetupapi
DELETE                      := @rm
MKDIR                       := @mkdir -p
COPY                        := @cp
MOVE                        := @mv
PRINT                       := @echo

# Define phony and suffix rules
.PHONY: all clean
.SUFFIXES:
.SUFFIXES: .cpp .c .o .h
vpath %.c ../../main/c/Posix ../../main/c/Windows

# Default build target does nothing
all :
	$(PRINT) You must specify a specify test to make!
clean :
	$(DELETE) -rf "$(BUILD_DIR)"

# Rule to create build directories
$(BUILD_DIR) :
	$(MKDIR) -p $@

# Build rules for all tests
testOpenClose : $(BUILD_DIR)/testOpenClose.o $(BUILD_DIR)/PosixHelperFunctions.o
	$(COMPILE) $(LDFLAGS) $(LIBRARIES) -o $@ $^
testRS485 : $(BUILD_DIR)/testRS485.o
	$(COMPILE) $(LDFLAGS) $(LIBRARIES) -o $@ $^
testEnumeratePosix : $(BUILD_DIR)/testEnumeratePosix.o $(BUILD_DIR)/PosixHelperFunctions.o
	$(COMPILE) $(LDFLAGS) $(LIBRARIES) -o $@ $^
testEnumerateWindows : $(BUILD_DIR)/testEnumerateWindows.o $(BUILD_DIR)/WindowsHelperFunctions.o
	$(COMPILE_WIN) $(LDFLAGS_WIN) $(LIBRARIES_WIN) -o $@ $^

# Suffix rules to get from *.c -> *.o
$(BUILD_DIR)/testEnumerateWindows.o : testEnumerateWindows.c
	$(MKDIR) $(BUILD_DIR)
	$(COPY) ../../main/c/Posix/*.h ../../main/c/
	$(COMPILE_WIN) $(INCLUDES) $(CFLAGS_WIN) -c $< -o $@
	$(DELETE) ../../main/c/*.h
$(BUILD_DIR)/WindowsHelperFunctions.o : WindowsHelperFunctions.c
	$(MKDIR) $(BUILD_DIR)
	$(COPY) ../../main/c/Posix/*.h ../../main/c/
	$(COMPILE_WIN) $(INCLUDES) $(CFLAGS_WIN) -c $< -o $@
	$(DELETE) ../../main/c/*.h
$(BUILD_DIR)/%.o : %.c
	$(MKDIR) $(BUILD_DIR)
	$(COMPILE) $(INCLUDES) $(CFLAGS) -c $< -o $@
