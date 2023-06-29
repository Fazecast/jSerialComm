ifeq ($(OS),Windows_NT)
	SHELL=cmd
	UPDATE_SCRIPT := updateVersionNumber.bat
	ROOT_DIR := $(shell cd)\..
	ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
		ARCH := amd64
	else
		ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
			ARCH := amd64
		else
			ARCH := aarch64
		endif
	endif
else
	ROOT_DIR := $(shell pwd)/..
	UPDATE_SCRIPT := ./updateVersionNumber.sh
	UNAME_P := $(shell uname -p)
	ifeq ($(UNAME_P),x86_64)
		ARCH := amd64
	else
		ARCH := aarch64
	endif
endif

clean :
	rm -rf ../build ../bin

bump :
	$(UPDATE_SCRIPT)

buildmeta :
	docker build --target build -t fazecast/jserialcomm:metabuilder-$(ARCH) .

build :
	docker build -t fazecast/jserialcomm:builder-$(ARCH) .

runmeta :
	docker run -it --privileged --rm fazecast/jserialcomm:metabuilder-$(ARCH)

run :
	docker run -it --privileged --rm fazecast/jserialcomm:builder-$(ARCH)

pushmeta :
	docker push fazecast/jserialcomm:metabuilder-$(ARCH)

push :
	docker push fazecast/jserialcomm:builder-$(ARCH)

combine :
	docker manifest rm fazecast/jserialcomm:builder
	docker manifest create fazecast/jserialcomm:builder --amend fazecast/jserialcomm:builder-amd64 --amend fazecast/jserialcomm:builder-aarch64
	docker manifest push fazecast/jserialcomm:builder

make :
	docker run --privileged --rm -v "$(ROOT_DIR)":/home/toolchain/jSerialComm fazecast/jserialcomm:builder
