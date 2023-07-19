@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION

REM Determine version number to which to update
FOR /F tokens^=2^ delims^=^" %%a in ('findstr /rc:"versionString = [^, ;]*" ..\src\main\java\com\fazecast\jSerialComm\SerialPort.java') DO SET oldVersion=%%a
FOR /F "tokens=1,2,3 delims=." %%a in ('echo %oldVersion%') DO (
	SET /a out=1+%%c
	SET newVersion=%%a.%%b.!out!
)
SET /p userInput=Desired version number [default=%newVersion%]: 
IF NOT [!userInput!] == [] SET newVersion=!userInput!
ECHO Using '!newVersion!'

REM Pull the latest library documentation
cd ..\site && git pull && cd ..

REM Update the version string throughout the library
@powershell -Command "(Get-Content src\main\java\com\fazecast\jSerialComm\SerialPort.java) | foreach-object {$_ -replace 'versionString = \"[.0-9]*','versionString = \"!newVersion!'} | Set-Content src\main\java\com\fazecast\jSerialComm\SerialPort.java"
@powershell -Command "(Get-Content src\main\java\com\fazecast\jSerialComm\package-info.java) | foreach-object {$_ -replace '@version .*','@version !newVersion!'} | Set-Content src\main\java\com\fazecast\jSerialComm\package-info.java"
@powershell -Command "(Get-Content build.gradle) | foreach-object {$_ -replace 'version = System.getenv.*','version = System.getenv(\"LIB_VERSION\") ?: \"!newVersion!\"'} | Set-Content build.gradle"
@powershell -Command "(Get-Content src\main\c\Posix\SerialPort_Posix.c) | foreach-object {$_ -replace 'nativeLibraryVersion\[\] = \"[.0-9]*\"','nativeLibraryVersion[] = \"!newVersion!\"'} | Set-Content src\main\c\Posix\SerialPort_Posix.c"
@powershell -Command "(Get-Content src\main\c\Windows\SerialPort_Windows.c) | foreach-object {$_ -replace 'nativeLibraryVersion\[\] = \"[.0-9]*\"','nativeLibraryVersion[] = \"!newVersion!\"'} | Set-Content src\main\c\Windows\SerialPort_Windows.c"
