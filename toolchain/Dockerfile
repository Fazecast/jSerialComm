# Set up the base image for building
FROM ubuntu:22.04 AS build
LABEL org.opencontainers.image.vendor="Fazecast, Inc."
LABEL org.opencontainers.image.authors="Will Hedgecock <will.hedgecock@fazecast.com>"
WORKDIR /home/toolchain
COPY external external
RUN echo 'APT::Install-Suggests "0";' >> /etc/apt/apt.conf.d/00-docker && \
    echo 'APT::Install-Recommends "0";' >> /etc/apt/apt.conf.d/00-docker && \
    addgroup toolchain && useradd -ms /bin/bash -g toolchain toolchain && \
    mkdir -p /home/toolchain && chown -R toolchain:toolchain /home/toolchain && \
    DEBIAN_FRONTEND=noninteractive apt update -y && apt upgrade -y && apt autoremove -y && \
    apt install --no-install-recommends -y gcc g++ gfortran libmpfr-dev libmpc-dev libgmp-dev \
                bison flex texinfo git openjdk-11-jdk-headless cmake clang llvm-dev libxml2-dev \
                uuid-dev libssl-dev bash patch make tar xz-utils bzip2 gzip sed cpio libbz2-dev \
                zlib1g-dev wget nano lld help2man unzip file gawk libtool-bin autoconf autogen && \
    wget http://crosstool-ng.org/download/crosstool-ng/crosstool-ng-1.25.0.tar.bz2 && \
    tar xjf crosstool-ng-1.25.0.tar.bz2 && rm -rf crosstool-ng-1.25.0.tar.bz2 && cd crosstool-ng-1.25.0 && \
    ./configure --prefix=/usr && make && make install && cd .. && rm -rf crosstool-ng-1.25.0
USER toolchain

# Build Mac OS Cross Compilers
RUN git clone https://github.com/tpoechtrager/osxcross.git && cd osxcross && \
    cp -f $HOME/external/MacOSX10.13.sdk.tar.xz tarballs/ && \
    echo | TARGET_DIR=$HOME/x-tools/osx32 OSX_VERSION_MIN=10.6 ./build.sh && \
    rm -rf $HOME/x-tools/osx32/bin/o64* && cd .. && rm -rf osxcross && \
    git clone https://github.com/tpoechtrager/osxcross.git && cd osxcross && \
    cp -f $HOME/external/MacOSX12.0.sdk.tar.xz tarballs/ && \
    echo | TARGET_DIR=$HOME/x-tools/osxcross OSX_VERSION_MIN=10.9 ./build.sh && \
    cd .. && rm -rf osxcross

# Build Solaris Cross Compilers
RUN mkdir -p $HOME/buildcc && cd $HOME/buildcc && export OLDPATH="$PATH" && \
    wget https://ftp.gnu.org/gnu/gcc/gcc-7.3.0/gcc-7.3.0.tar.gz && \
    wget https://ftp.gnu.org/gnu/binutils/binutils-2.30.tar.gz && cd $HOME/x-tools && \
    tar xvhf $HOME/external/SolarisSystemHeaders.tgz && cd $HOME/buildcc && \
    export TARGET=x86_64-sun-solaris2.10 && export PREFIX="$HOME/x-tools/$TARGET" && \
    export SYSROOT="$PREFIX/sysroot" && export PATH="$PREFIX/bin:$PATH" && \
    tar xvhf binutils-2.30.tar.gz && mkdir build-binutils && cd build-binutils && \
    ../binutils-2.30/configure --prefix="$PREFIX" --target="$TARGET" --with-sysroot="$SYSROOT" --disable-nls --disable-werror && \
    make && make install && cd .. && rm -rf build-binutils binutils-2.30 && \
    tar xvhf gcc-7.3.0.tar.gz && mkdir build-gcc && cd build-gcc && \
    ../gcc-7.3.0/configure --prefix="$PREFIX" --target="$TARGET" --with-sysroot="$SYSROOT" --disable-nls --enable-languages=c --with-gnu-as --with-gnu-ld && \
    make all-gcc all-target-libgcc && make install-gcc install-target-libgcc && cd .. && rm -rf build-gcc gcc-7.3.0 && \
    export TARGET=sparc-sun-solaris2.10 && export PREFIX="$HOME/x-tools/$TARGET" && \
    export SYSROOT="$PREFIX/sysroot" && export PATH="$PREFIX/bin:$PATH" && \
    tar xvhf binutils-2.30.tar.gz && mkdir build-binutils && cd build-binutils && \
    ../binutils-2.30/configure --prefix="$PREFIX" --target="$TARGET" --with-sysroot="$SYSROOT" --disable-nls --disable-werror && \
    make && make install && cd .. && rm -rf build-binutils binutils-2.30 && \
    tar xvhf gcc-7.3.0.tar.gz && mkdir build-gcc && cd build-gcc && \
    ../gcc-7.3.0/configure --prefix="$PREFIX" --target="$TARGET" --with-sysroot="$SYSROOT" --disable-nls --enable-languages=c --with-gnu-as --with-gnu-ld && \
    make all-gcc all-target-libgcc && make install-gcc install-target-libgcc && cd $HOME && rm -rf buildcc && \
    unset TARGET && unset PREFIX && unset SYSROOT && export PATH="$OLDPATH"

# Build BSD Cross Compilers
RUN mkdir -p $HOME/x-tools/x86_64-unknown-freebsd11.2 && cd $HOME/x-tools/x86_64-unknown-freebsd11.2 && \
    tar -xf $HOME/external/FreeBSD-amd64-base.txz ./lib/ ./usr/lib/ ./usr/include/ && \
    mkdir -p $HOME/x-tools/arm64-unknown-freebsd11.2 && cd $HOME/x-tools/arm64-unknown-freebsd11.2 && \
    tar -xf $HOME/external/FreeBSD-arm64-base.txz ./lib/ ./usr/lib/ ./usr/include/ && \
    mkdir -p $HOME/x-tools/i386-unknown-freebsd11.2 && cd $HOME/x-tools/i386-unknown-freebsd11.2 && \
    tar -xf $HOME/external/FreeBSD-i386-base.txz ./lib/ ./usr/lib/ ./usr/include/ && \
    mkdir -p $HOME/x-tools/amd64-unknown-openbsd6.2 && cd $HOME/x-tools/amd64-unknown-openbsd6.2 && \
    tar -xf $HOME/external/OpenBSD-amd64-base62.tgz ./usr/lib/ ./usr/include/ && \
    tar -xf $HOME/external/OpenBSD-amd64-comp62.tgz ./usr/lib/ ./usr/include/ && \
    mkdir -p $HOME/x-tools/i386-unknown-openbsd6.2 && cd $HOME/x-tools/i386-unknown-openbsd6.2 && \
    tar -xf $HOME/external/OpenBSD-i386-base62.tgz ./usr/lib/ ./usr/include/ && \
    tar -xf $HOME/external/OpenBSD-i386-comp62.tgz ./usr/lib/ ./usr/include/ && cd $HOME

# Build Linux Cross Compilers
RUN mkdir -p $HOME/conffiles && cd $HOME/conffiles && tar xvf $HOME/external/CrosstoolNgConfigFiles.tgz && \
    mkdir -p $HOME/build/.build/tarballs && cd $HOME/build/.build/tarballs && \
    wget https://zlib.net/fossils/zlib-1.2.12.tar.gz && cd ../.. && \
    cp -f $HOME/conffiles/jSerialComm32.config .config && ct-ng build && \
    cp -f $HOME/conffiles/jSerialComm32HF.config .config && ct-ng build && \
    cp -f $HOME/conffiles/jSerialComm64.config .config && ct-ng build && \
    cp -f $HOME/conffiles/jSerialCommPPC64LE.config .config && ct-ng build && \
    cp -f $HOME/conffiles/jSerialCommx86.config .config && ct-ng build && \
    cp -f $HOME/conffiles/jSerialCommx86_64.config .config && ct-ng build && \
    cd $HOME && rm -rf build conffiles

# Build Windows Cross Compilers
RUN mkdir -p $HOME/x-tools/windows/bin && cd $HOME/x-tools/windows && tar xvf $HOME/external/WindowsHeaders.tgz && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x64/kernel32.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x64/kernel32.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x86/kernel32.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x86/kernel32.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm/kernel32.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm/kernel32.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm64/kernel32.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm64/kernel32.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x64/AdvAPI32.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x64/AdvAPI32.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x86/AdvAPI32.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x86/AdvAPI32.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm/AdvAPI32.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm/AdvAPI32.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm64/AdvAPI32.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm64/AdvAPI32.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x64/SetupAPI.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x64/SetupAPI.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x86/SetupAPI.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x86/SetupAPI.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm/SetupAPI.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm/SetupAPI.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm64/SetupAPI.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm64/SetupAPI.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x64/Uuid.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x64/uuid.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x86/Uuid.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x86/uuid.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm/Uuid.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm/uuid.lib && \
    mv $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm64/Uuid.Lib $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm64/uuid.lib && \
    cp $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/shared/driverspecs.h $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/shared/DriverSpecs.h && \
    cp $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/um/WinBase.h $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/um/winbase.h && \
    cp $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/um/WinUser.h $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/um/winuser.h && \
    cp $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/um/WinNls.h $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/um/winnls.h && \
    cp $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/shared/specstrings.h $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/shared/SpecStrings.h && \
    cp $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/um/CommCtrl.h $HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/um/commctrl.h && \
    cd bin && echo 'clang -target x86_64-pc-win32-msvc -fmsc-version=1935 -fms-extensions -fdelayed-template-parsing -fexceptions -mthread-model posix -fno-threadsafe-statics -Wno-microsoft-anon-tag -Wno-pragma-pack -Wno-ignored-pragma-intrinsic -Wno-ignored-attributes -Wno-void-pointer-to-int-cast -Wno-int-to-void-pointer-cast -DWIN32 -D_WIN32 -D_MT -D_CHAR_UNSIGNED -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -U__GNUC__ -U__gnu_linux__ -U__GNUC_MINOR__ -U__GNUC_PATCHLEVEL__ -U__GNUC_STDC_INLINE__ -I$HOME/x-tools/windows/msvc/MSVC/14.35.32215/include -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/ucrt -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/um -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/shared -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/winrt "$@"' > x86_64-pc-win32-msvc-gcc && \
    echo 'clang -target i686-pc-win32-msvc -fmsc-version=1935 -fms-extensions -fdelayed-template-parsing -fexceptions -mthread-model posix -fno-threadsafe-statics -Wno-microsoft-anon-tag -Wno-pragma-pack -Wno-ignored-pragma-intrinsic -Wno-ignored-attributes -Wno-void-pointer-to-int-cast -Wno-int-to-void-pointer-cast -DWIN32 -D_WIN32 -D_MT -D_CHAR_UNSIGNED -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -U__GNUC__ -U__gnu_linux__ -U__GNUC_MINOR__ -U__GNUC_PATCHLEVEL__ -U__GNUC_STDC_INLINE__ -I$HOME/x-tools/windows/msvc/MSVC/14.35.32215/include -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/ucrt -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/um -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/shared -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/winrt "$@"' > i686-pc-win32-msvc-gcc && \
    echo 'clang -target arm-pc-win32-msvc -fmsc-version=1935 -fms-extensions -fdelayed-template-parsing -fexceptions -mthread-model posix -fno-threadsafe-statics -Wno-microsoft-anon-tag -Wno-pragma-pack -Wno-ignored-pragma-intrinsic -Wno-ignored-attributes -Wno-void-pointer-to-int-cast -Wno-int-to-void-pointer-cast -DWIN32 -D_WIN32 -D_MT -D_CHAR_UNSIGNED -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -U__GNUC__ -U__gnu_linux__ -U__GNUC_MINOR__ -U__GNUC_PATCHLEVEL__ -U__GNUC_STDC_INLINE__ -I$HOME/x-tools/windows/msvc/MSVC/14.35.32215/include -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/ucrt -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/um -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/shared -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/winrt "$@"' > arm-pc-win32-msvc-gcc && \
    echo 'clang -target aarch64-pc-win32-msvc -fmsc-version=1935 -fms-extensions -fdelayed-template-parsing -fexceptions -mthread-model posix -fno-threadsafe-statics -Wno-microsoft-anon-tag -Wno-pragma-pack -Wno-ignored-pragma-intrinsic -Wno-ignored-attributes -Wno-void-pointer-to-int-cast -Wno-int-to-void-pointer-cast -DWIN32 -D_WIN32 -D_MT -D_CHAR_UNSIGNED -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -U__GNUC__ -U__gnu_linux__ -U__GNUC_MINOR__ -U__GNUC_PATCHLEVEL__ -U__GNUC_STDC_INLINE__ -I$HOME/x-tools/windows/msvc/MSVC/14.35.32215/include -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/ucrt -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/um -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/shared -I$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Include/winrt "$@"' > aarch64-pc-win32-msvc-gcc && \
    echo 'clang -fuse-ld=lld -target x86_64-pc-win32-msvc -Wl,-machine:x64 -fmsc-version=1935 -L$HOME/x-tools/windows/msvc/MSVC/14.35.32215/lib/x64 -L$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x64 -L$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/ucrt/x64 -nostdlib -llibcmt -Wno-msvc-not-found "$@"' > x86_64-pc-win32-msvc-ld && \
    echo 'clang -fuse-ld=lld -target i686-pc-win32-msvc -Wl,-machine:x86 -fmsc-version=1935 -L$HOME/x-tools/windows/msvc/MSVC/14.35.32215/lib/x86 -L$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/x86 -L$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/ucrt/x86 -nostdlib -llibcmt -Wno-msvc-not-found "$@"' > i686-pc-win32-msvc-ld && \
    echo 'clang -fuse-ld=lld -target arm-pc-win32-msvc -Wl,-machine:arm -fmsc-version=1935 -L$HOME/x-tools/windows/msvc/MSVC/14.35.32215/lib/arm -L$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm -L$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/ucrt/arm -nostdlib -llibcmt -Wno-msvc-not-found "$@"' > arm-pc-win32-msvc-ld && \
    echo 'clang -fuse-ld=lld -target aarch64-pc-win32-msvc -Wl,-machine:arm64 -fmsc-version=1935 -L$HOME/x-tools/windows/msvc/MSVC/14.35.32215/lib/arm64 -L$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/um/arm64 -L$HOME/x-tools/windows/msvc/Kits/10.0.22000.0/Lib/ucrt/arm64 -nostdlib -llibcmt -Wno-msvc-not-found "$@"' > aarch64-pc-win32-msvc-ld && \
    chmod +x * && cd $HOME

# Install Gradle
RUN mkdir -p $HOME/gradle && unzip -d $HOME/gradle $HOME/external/gradle-8.1.1-bin.zip


# Set up the base image for the final toolchain
FROM ubuntu:22.04
LABEL org.opencontainers.image.vendor="Fazecast, Inc."
LABEL org.opencontainers.image.authors="Will Hedgecock <will.hedgecock@fazecast.com>"
RUN echo 'APT::Install-Suggests "0";' >> /etc/apt/apt.conf.d/00-docker && \
    echo 'APT::Install-Recommends "0";' >> /etc/apt/apt.conf.d/00-docker && \
    addgroup toolchain && useradd -ms /bin/bash -g toolchain toolchain && \
    mkdir -p /home/toolchain/gradle && mkdir -p /home/toolchain/x-tools && \
    chown -R toolchain:toolchain /home/toolchain && \
    DEBIAN_FRONTEND=noninteractive apt update -y && apt upgrade -y && apt autoremove -y && \
    apt install --no-install-recommends -y openjdk-11-jdk-headless make clang lld \
    llvm-dev libmpc-dev && apt clean && rm -rf /var/lib/apt/lists/*
COPY --from=build /home/toolchain/x-tools /home/toolchain/x-tools
COPY --from=build /home/toolchain/gradle /home/toolchain/gradle

# Create compilation script and entry point
USER toolchain
WORKDIR /home/toolchain
RUN echo '#!/bin/sh\n' >> compile.sh && \
    echo 'valid_targets="all linux arm powerpc solaris freebsd openbsd osx win32 win64 winarm winarm64 "' >> compile.sh && \
    echo 'posix_targets="linux arm powerpc solaris freebsd openbsd osx "' >> compile.sh && \
    echo 'win_targets="win32 win64 winarm winarm64 "\n' >> compile.sh && \
    echo 'if [ $# -ne 1 ] || ! echo "$valid_targets" | grep -q "$1 "; then echo "Target must be one of: $valid_targets"; return 1; fi\n' >> compile.sh && \
    echo 'if [ "$1" = "all" ]; then cd jSerialComm/src/main/c/Posix && make && cd ../Windows && make;' >> compile.sh && \
    echo 'elif echo "$posix_targets" | grep -q "$1 "; then cd jSerialComm/src/main/c/Posix && make;' >> compile.sh && \
    echo 'else cd jSerialComm/src/main/c/Windows && make;' >> compile.sh && \
    echo 'fi' >> compile.sh && chmod +x compile.sh
ENV PATH="/home/toolchain/x-tools/aarch64-unknown-linux-gnu/bin:/home/toolchain/x-tools/amd64-unknown-openbsd6.2/bin:/home/toolchain/x-tools/arm-unknown-linux-gnueabi/bin:/home/toolchain/x-tools/arm-unknown-linux-gnueabihf/bin:/home/toolchain/x-tools/arm64-unknown-freebsd11.2/bin:/home/toolchain/x-tools/i386-unknown-freebsd11.2/bin:/home/toolchain/x-tools/i386-unknown-openbsd6.2/bin:/home/toolchain/x-tools/i486-unknown-linux-gnu/bin:/home/toolchain/x-tools/powerpc64le-unknown-linux-gnu/bin:/home/toolchain/x-tools/x86_64-sun-solaris2.10/bin:/home/toolchain/x-tools/sparc-sun-solaris2.10/bin:/home/toolchain/x-tools/osxcross/bin:/home/toolchain/x-tools/osx32/bin:/home/toolchain/x-tools/x86_64-unknown-linux-gnu/bin:/home/toolchain/x-tools/x86_64-unknown-freebsd11.2/bin:/home/toolchain/x-tools/windows/bin:/home/toolchain/gradle/gradle-8.1.1/bin:$PATH"
ENTRYPOINT [ "/home/toolchain/compile.sh" ]
CMD [ "all" ]

#elif [ "$1" = "package" ]; then cd jSerialComm && gradle build;