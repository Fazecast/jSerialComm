#!/bin/sh

# Determine version number to which to update
oldVersion=$(grep -o "versionString = [^, ;]*" ../src/main/java/com/fazecast/jSerialComm/SerialPort.java | grep -o "\".*\"" | grep -o [^\"].*[^\"])
newVersion=$(echo $oldVersion | cut -d. -f1,2).$((1 + $(echo $oldVersion | cut -d. -f3)))
echo "Desired version number [default=$newVersion]: "
read userInput
if [ -n "$userInput" ]; then newVersion="$userInput"; fi
echo "Using '"$newVersion"'"

# Pull the latest library documentation
cd ../site && git pull && cd ..

# Update the version string throughout the library
sed -i "s/versionString = [^, ;]*/versionString = \"$newVersion\"/" src/main/java/com/fazecast/jSerialComm/SerialPort.java
sed -i "s/@version .*/@version $newVersion/" src/main/java/com/fazecast/jSerialComm/package-info.java
sed -i "s/version = System.getenv(\"LIB_VERSION\") ?: .*/version = System.getenv(\"LIB_VERSION\") ?: '$newVersion'/" build.gradle
