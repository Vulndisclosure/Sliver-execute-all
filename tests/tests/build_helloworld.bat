@echo off
setlocal

set GOARCH=386
@echo on
go build -o helloworld_go.x86.exe helloworld.go
@REM go build -ldflags "-s" -o helloworld_go.x86.exe helloworld.go
@echo off

set GOARCH=amd64
@echo on
go build -o helloworld_go.x64.exe helloworld.go
@REM go build -ldflags "-s" -o helloworld_go.x64.exe helloworld.go
@echo off
set GOARCH=

call :VCVARSALL x86
if errorlevel 1 echo Failed to initialize x86 environment. & exit /b 1

@echo on
cl helloworld.c /O2 /Fe:helloworld_c.x86.exe /link /DEBUG /OPT:REF /OPT:ICF
cl helloworld_seh.c /O2 /Fe:helloworld_seh_c.x86.exe /link /DEBUG /OPT:REF /OPT:ICF
@REM cl helloworld.c /O2 /Fe:helloworld_c.x86.exe
@REM cl helloworld_seh.c /O2 /Fe:helloworld_seh_c.x86.exe
@echo off

call :VCVARSALL x64
if errorlevel 1 echo Failed to initialize x64 environment. & exit /b 1

@echo on
cl helloworld.c /O2 /Fe:helloworld_c.x64.exe /link /DEBUG /OPT:REF /OPT:ICF
cl helloworld_seh.c /O2 /Fe:helloworld_seh_c.x64.exe /link /DEBUG /OPT:REF /OPT:ICF
@REM cl helloworld.c /O2 /Fe:helloworld_c.x64.exe
@REM cl helloworld_seh.c /O2 /Fe:helloworld_seh_c.x64.exe
@echo off

del helloworld.obj
del helloworld_seh.obj

endlocal

exit /b 0

REM Copied from https://github.com/dotnet/runtime/blob/v8.0.0/src/coreclr/nativeaot/BuildIntegration/findvcvarsall.bat
:VCVARSALL

IF "%~1"=="" (
    ECHO Usage: VCVARSALL ^<arch^>
    EXIT /B 1
)

SET vswherePath=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
IF NOT EXIST "%vswherePath%" (
    ECHO vswhere.exe not found.
    EXIT /B 1
)

SET toolsSuffix=x86.x64
IF /I "%~1"=="arm64" SET toolsSuffix=ARM64

FOR /F "tokens=*" %%i IN (
    '"%vswherePath%" -latest -prerelease -products * ^
    -requires Microsoft.VisualStudio.Component.VC.Tools.%toolsSuffix% ^
    -version [16^,18^) ^
    -property installationPath'
) DO SET vsBase=%%i

IF "%vsBase%"=="" (
    ECHO Visual Studio not found.
    EXIT /B 1
)

SET procArch=%PROCESSOR_ARCHITEW6432%
IF "%procArch%"=="" SET procArch=%PROCESSOR_ARCHITECTURE%

SET vcEnvironment=%~1
IF /I "%~1"=="x64" (
    SET vcEnvironment=x86_amd64
    IF /I "%procArch%"=="AMD64" SET vcEnvironment=amd64
)
IF /I "%~1"=="arm64" (
    SET vcEnvironment=x86_arm64
    IF /I "%procArch%"=="AMD64" SET vcEnvironment=amd64_arm64
)

CALL "%vsBase%\vc\Auxiliary\Build\vcvarsall.bat" %vcEnvironment% > NUL

IF "%LIB%"=="" (
    ECHO Failed to initialize VC environment.
    EXIT /B 1
)

EXIT /B 0

GOTO :EOF
