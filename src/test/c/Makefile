# Compiler tools, commands, and flags
COMPILE                     = gcc
COMPILE_WIN                 = cl
LINK						= gcc
LINK_WIN					= link
BUILD_DIR                   = build
JDK_HOME                    = $(shell if [ "`uname`" = "Darwin" ]; then echo "`/usr/libexec/java_home`"; else echo "$$JDK_HOME"; fi)
INCLUDES                    = -I"../../main/c/Posix" -I"../../main/c/Windows" -I"$(JDK_HOME)/include" -I"$(JDK_HOME)/include/win32" -I"$(JDK_HOME)/include/linux" -I"$(JDK_HOME)/include/darwin" -I"$(JDK_HOME)/include/solaris"
CFLAGS                      = -fPIC -O0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 $(shell if [ "`uname`" != "Darwin" ]; then echo "-static-libgcc"; fi)
CFLAGS_WIN                  = /c /O2 /GF /GL /MT /EHsc /fp:precise /J /nologo /TC /Zi
LDFLAGS                     = -O0 $(shell if [ "`uname`" != "Darwin" ]; then echo "-static-libgcc"; fi)
LDFLAGS_WIN                 = /LTCG /NOLOGO /INCREMENTAL:NO /DEBUG /OPT:REF,ICF,LBR
LIBRARIES                   = $(shell if [ "`uname`" = "Darwin" ]; then echo "-framework Cocoa -framework IOKit"; else echo "-pthread"; fi)
LIBRARIES_WIN               = Advapi32.lib SetupAPI.lib Shell32.lib
DELETE                      = @rm
MKDIR                       = @mkdir -p
COPY                        = @cp
MOVE                        = @mv
PRINT                       = @echo

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
	$(DELETE) *.pdb *.exe

# Rule to create build directories
$(BUILD_DIR) :
	$(MKDIR) -p $@

# Build rules for all tests
testOpenClose : $(BUILD_DIR)/testOpenClose.o $(BUILD_DIR)/PosixHelperFunctions.o
	$(LINK) $(LDFLAGS) $(LIBRARIES) -o $@ $^
testRS485 : $(BUILD_DIR)/testRS485.o
	$(LINK) $(LDFLAGS) $(LIBRARIES) -o $@ $^
testEnumeratePosix : $(BUILD_DIR)/testEnumeratePosix.o $(BUILD_DIR)/PosixHelperFunctions.o
	$(LINK) $(LDFLAGS) $(LIBRARIES) -o $@ $^
testEnumerateWindows : $(BUILD_DIR)/testEnumerateWindows.obj $(BUILD_DIR)/WindowsHelperFunctions.obj
	$(LINK_WIN) $(LDFLAGS_WIN) /OUT:$@.exe $^ $(LIBRARIES_WIN)

# Suffix rules to get from *.c -> *.o
$(BUILD_DIR)/testEnumerateWindows.obj : testEnumerateWindows.c
	$(MKDIR) $(BUILD_DIR)
	$(COPY) ../../main/c/Posix/*.h ../../main/c/
	$(COMPILE_WIN) $(INCLUDES) $(CFLAGS_WIN) -c $< -Fo$@
	$(DELETE) ../../main/c/*.h
$(BUILD_DIR)/WindowsHelperFunctions.obj : WindowsHelperFunctions.c
	$(MKDIR) $(BUILD_DIR)
	$(COPY) ../../main/c/Posix/*.h ../../main/c/
	$(COMPILE_WIN) $(INCLUDES) $(CFLAGS_WIN) -c $< -Fo$@
	$(DELETE) ../../main/c/*.h
$(BUILD_DIR)/%.o : %.c
	$(MKDIR) $(BUILD_DIR)
	$(COMPILE) $(INCLUDES) $(CFLAGS) -c $< -o $@
