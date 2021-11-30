# Compiler tools, commands, and flags
COMPILE                     := gcc
BUILD_DIR                   := build
JDK_HOME                    := $(shell if [ "`uname`" = "Darwin" ]; then echo "`/usr/libexec/java_home`"; else echo "$$JDK_HOME"; fi)
INCLUDES                    := -I"../../main/c/Posix" -I"$(JDK_HOME)/include" -I"$(JDK_HOME)/include/linux" -I"$(JDK_HOME)/include/darwin" -I"$(JDK_HOME)/include/solaris"
CFLAGS                      := -fPIC -Os -flto -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 $(shell if [ "`uname`" != "Darwin" ]; then echo "-static-libgcc -fuse-linker-plugin"; fi)
LDFLAGS                     := -Os -flto -fuse-linker-plugin $(shell if [ "`uname`" != "Darwin" ]; then echo "-static-libgcc -s"; fi)
LIBRARIES                   := $(shell if [ "`uname`" = "Darwin" ]; then echo "-framework Cocoa -framework IOKit"; else echo "-pthread"; fi)
DELETE                      := @rm
MKDIR                       := @mkdir -p
COPY                        := @cp
MOVE                        := @mv
PRINT                       := @echo

# Define phony and suffix rules
.PHONY: all clean
.SUFFIXES:
.SUFFIXES: .cpp .c .o .h
vpath %.c ../../main/c/Posix

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

# Suffix rules to get from *.c -> *.o
$(BUILD_DIR)/%.o : %.c
	$(MKDIR) $(BUILD_DIR)
	$(COMPILE) $(INCLUDES) $(CFLAGS) -c $< -o $@
