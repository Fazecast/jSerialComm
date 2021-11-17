# Compiler tools, commands, and flags
COMPILE                     := gcc
BUILD_DIR                   := build
INCLUDES                    := -I"../../main/c/Posix" -I"$(JDK_HOME)/include" -I"$(JDK_HOME)/include/linux" -I"$(JDK_HOME)/include/darwin" -I"$(JDK_HOME)/include/solaris"
CFLAGS                      := -fPIC -Os -flto -static-libgcc -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fuse-linker-plugin
LDFLAGS                     := -Os -flto -static-libgcc -shared -fuse-linker-plugin -s
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
	$(COMPILE) $(LDFLAGS) -o $@ $<

# Suffix rules to get from *.c -> *.o
$(BUILD_DIR)/%.o : %.c
	$(MKDIR) $(BUILD_DIR)
	$(COMPILE) $(INCLUDES) $(CFLAGS) -c $< -o $@
