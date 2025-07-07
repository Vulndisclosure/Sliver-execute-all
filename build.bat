@echo off
setlocal EnableDelayedExpansion

set CC_FLAGS=/nologo /O1 /Os /Oi /GS- /Zc:inline /permissive- /W3 /D _NO_CRT_STDIO_INLINE /D _CRT_SECURE_NO_WARNINGS
set BOF_FLAGS=%CC_FLAGS% /Gy- /GF- /D BOF
set C2SC_LIB_FLAGS=kernel32.lib shlwapi.lib
set C2SC_LINK_FLAGS=%C2SC_LIB_FLAGS% libucrt.lib /link /NOLOGO /LIBPATH:tools\link /OPT:REF /OPT:ICF /EMITPOGOPHASEINFO /ALIGN:16 /SUBSYSTEM:CONSOLE /BASE:0 /IGNORE:4108,4254,4281
set C2SC_LINK_FLAGS32=msvcrt.x86.lib %C2SC_LINK_FLAGS%
set C2SC_LINK_FLAGS64=msvcrt.x64.lib %C2SC_LINK_FLAGS%

if not exist "bin" mkdir "bin"

call :SETUP_ENV

call :SET_ENV MSVC_X86
cl src\hooks\hooks.cc src\hooks\control.cc src\hooks\vfs.cc src\hooks\log.cc src\hooks\pipeserver.cc src\hooks\simplelist1.cc src\hooks\simplelist2.cc %CC_FLAGS% /Fe"bin\hooks.x86.exe" %C2SC_LINK_FLAGS32% /ENTRY:HookControlEntry
cl src\hooks\hooks.cc src\hooks\control.cc src\hooks\vfs.cc src\hooks\log.cc src\hooks\pipeserver.cc src\hooks\simplelist1.cc src\hooks\simplelist2.cc %CC_FLAGS% /D _DEBUG /Fe"bin\hooks.x86d.exe" %C2SC_LINK_FLAGS32% /ENTRY:HookControlEntry
cl src\c2scstartup.cc src\fileserver.cc src\globalshareddata.cc %CC_FLAGS% /Fe"bin\fileserver.x86.exe" %C2SC_LINK_FLAGS32% /ENTRY:Entry
cl src\c2scstartup.cc src\fileserver.cc src\globalshareddata.cc %CC_FLAGS% /D _DEBUG /Fe"bin\fileserver.x86d.exe" %C2SC_LINK_FLAGS32% /ENTRY:Entry

call :SET_ENV MSVC_X64
ml64 /nologo /c src/selfunload.x64.asm
cl src\hooks\hooks.cc src\hooks\control.cc src\hooks\vfs.cc src\hooks\log.cc src\hooks\pipeserver.cc src\hooks\simplelist1.cc src\hooks\simplelist2.cc selfunload.x64.obj %CC_FLAGS% /Fe"bin\hooks.x64.exe" %C2SC_LINK_FLAGS64% /ENTRY:HookControlEntry
cl src\hooks\hooks.cc src\hooks\control.cc src\hooks\vfs.cc src\hooks\log.cc src\hooks\pipeserver.cc src\hooks\simplelist1.cc src\hooks\simplelist2.cc selfunload.x64.obj %CC_FLAGS% /D _DEBUG /Fe"bin\hooks.x64d.exe" %C2SC_LINK_FLAGS64% /ENTRY:HookControlEntry
cl src\c2scstartup.cc src\fileserver.cc src\globalshareddata.cc selfunload.x64.obj %CC_FLAGS% /Fe"bin\fileserver.x64.exe" %C2SC_LINK_FLAGS64% /ENTRY:Entry
cl src\c2scstartup.cc src\fileserver.cc src\globalshareddata.cc selfunload.x64.obj %CC_FLAGS% /D _DEBUG /Fe"bin\fileserver.x64d.exe" %C2SC_LINK_FLAGS64% /ENTRY:Entry

cl /nologo tools\c2sc.c /Fe"tools\c2sc.exe"
tools\c2sc.exe hs bin\hooks.x86.exe bin\hooks.x86d.exe bin\hooks.x64.exe bin\hooks.x64d.exe
move hooks_*.h src\hooks
tools\c2sc.exe b bin\fileserver.x86.exe bin\fileserver.x86d.exe bin\fileserver.x64.exe bin\fileserver.x64d.exe
move fileserver.*.bin bin
del tools\c2sc.exe bin\hooks.*.exe bin\fileserver.*.exe *.obj

call :SET_ENV MSVC_X86
cl /c src\inlineexecutepe.cc %BOF_FLAGS% /Fo"bin\inlineexecutepe.x86.o"
cl /c src\inlineexecutepe.cc %BOF_FLAGS% /D _DEBUG /Fo"bin\inlineexecutepe.x86d.o"
cl /c src\inlinelogpe.cc %BOF_FLAGS% /Fo"bin\inlinelogpe.x86.o"
cl /c src\inlinelogpe.cc %BOF_FLAGS% /D _DEBUG /Fo"bin\inlinelogpe.x86d.o"
cl /c src\inlinelistpe.cc %BOF_FLAGS% /Fo"bin\inlinelistpe.x86.o"
cl /c src\inlinelistpe.cc %BOF_FLAGS% /D _DEBUG /Fo"bin\inlinelistpe.x86d.o"

call :SET_ENV MSVC_X64
cl /c src\inlineexecutepe.cc %BOF_FLAGS% /Fo"bin\inlineexecutepe.x64.o"
cl /c src\inlineexecutepe.cc %BOF_FLAGS% /D _DEBUG /Fo"bin\inlineexecutepe.x64d.o"
cl /c src\inlinelogpe.cc %BOF_FLAGS% /Fo"bin\inlinelogpe.x64.o"
cl /c src\inlinelogpe.cc %BOF_FLAGS% /D _DEBUG /Fo"bin\inlinelogpe.x64d.o"
cl /c src\inlinelistpe.cc %BOF_FLAGS% /Fo"bin\inlinelistpe.x64.o"
cl /c src\inlinelistpe.cc %BOF_FLAGS% /D _DEBUG /Fo"bin\inlinelistpe.x64d.o"

cl /nologo /O2 /std:c++20 /EHsc tools\fixbof.cc tools\obj.cc /Fe"tools\fixbof.exe"
tools\fixbof.exe tools\link ^
bin\inlineexecutepe.x86.o bin\inlineexecutepe.x86d.o bin\inlineexecutepe.x64.o bin\inlineexecutepe.x64d.o ^
bin\inlinelogpe.x86.o bin\inlinelogpe.x86d.o bin\inlinelogpe.x64.o bin\inlinelogpe.x64d.o ^
bin\inlinelistpe.x86.o bin\inlinelistpe.x86d.o bin\inlinelistpe.x64.o bin\inlinelistpe.x64d.o
del tools\fixbof.exe *.obj

copy /Y src\inline-execute-all.cna bin\inline-execute-all.cna
if not exist "bin\inline-execute-all" mkdir "bin\inline-execute-all"
move /Y bin\*.o bin\inline-execute-all
move /Y bin\*.bin bin\inline-execute-all

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
