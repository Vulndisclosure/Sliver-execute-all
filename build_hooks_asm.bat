@echo off
setlocal EnableDelayedExpansion

call :SETUP_ENV

call :SET_ENV MSVC_X86
ml /nologo /Fo src\hooks.x86.obj /c src\hooks.x86.asm

call :SET_ENV MSVC_X64
ml64 /nologo /Fo src\hooks.x64.obj /c src\hooks.x64.asm

cl /nologo tools\strip.c /Fe"tools\strip.exe"
tools\strip.exe src\hooks.x86.obj src\hooks.x64.obj
del tools\strip.exe strip.obj

endlocal

exit /b 0


:: SETUP_ENV Function - Initializes environment variables for x86 and x64 builds
:: Usage: CALL :SETUP_ENV
:SETUP_ENV

rem Save the current environment variables
call :SAVE_ENV BASE

rem Initialize x86 environments
call :VCVARSALL x86
if errorlevel 1 echo Failed to initialize x86 environment. & exit /b 1
call :SAVE_ENV MSVC_X86

rem Initialize x64 environments
call :VCVARSALL x64
if errorlevel 1 echo Failed to initialize x64 environment. & exit /b 1
call :SAVE_ENV MSVC_X64

rem Restore the original environment variables
call :SET_ENV BASE

goto :EOF


:: SAVE_ENV Function - Backs up current environment variables with a specified name prefix
:: Usage: CALL :SAVE_ENV MyBackup
:SAVE_ENV
if "%~1"=="" (
    echo Please provide a backup name.
    goto :EOF
)

for /f "tokens=1,* delims==" %%A in ('set') do (
    rem Check if the variable already starts with __ENV_, skip if true
    set "T=%%A"
    if /i "!T:~0,6!" neq "__ENV_" (
        set "__ENV_%~1_%%A=%%B"
    )
)
set T=
goto :EOF


:: SET_ENV Function - Clears non-__ENV_ variables, then restores from the specified name prefix
:: Usage: CALL :SET_ENV MyBackup
:SET_ENV
if "%~1"=="" (
    echo Please provide a backup name.
    goto :EOF
)

:: Step 1: Clear all non-__ENV_ variables
for /f "tokens=1,* delims==" %%A in ('set') do (
	set "T=%%A"
    if /i "!T:~0,6!" neq "__ENV_" (
        set "%%A="
    )
)
set T=

:: Step 2: Restore variables with the specified prefix
set "P=__ENV_%~1_"
for /f "tokens=1,* delims==" %%A in ('set %P%') do (
    set "N=%%A"
    set "V=%%B"
	
	rem Extract original variable name by removing prefix
	set "N=!N:%P%=!"

    rem Restore the environment variable
    set "!N!=!V!"
)
set P=
set N=
set V=
goto :EOF


@REM Copied from https://github.com/dotnet/runtime/blob/v8.0.0/src/coreclr/nativeaot/BuildIntegration/findvcvarsall.bat
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
