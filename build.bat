@echo off
setlocal

set COMMON_FLAGS=/nologo /O1 /Os /GS- /Zc:inline /permissive- /std:c++17 /W3 /D JM_XORSTR_DISABLE_AVX_INTRINSICS
set HOOKS_LINK_FLAGS=/link /NOLOGO /OPT:REF /ENTRY:__HookControlEntry /ORDER:@hooksorder.txt /SUBSYSTEM:CONSOLE
set BOF_FLAGS=%COMMON_FLAGS% /Gy- /GF- /D BOF

call :VCVARSALL x86
if errorlevel 1 echo Failed to initialize x86 environment. & exit /b 1

@echo on
cl src\hooks.cpp src\hooks.x86.obj %COMMON_FLAGS% /Fe"bin\hooks.x86.exe" %HOOKS_LINK_FLAGS%
cl src\hooks.cpp src\hooks.x86.obj %COMMON_FLAGS% /D _DEBUG /Fe"bin\hooks.x86d.exe" %HOOKS_LINK_FLAGS%
@echo off

call :VCVARSALL x64
if errorlevel 1 echo Failed to initialize x64 environment. & exit /b 1

@echo on
cl src\hooks.cpp src\hooks.x64.obj %COMMON_FLAGS% /Fe"bin\hooks.x64.exe" %HOOKS_LINK_FLAGS%
cl src\hooks.cpp src\hooks.x64.obj %COMMON_FLAGS% /D _DEBUG /Fe"bin\hooks.x64d.exe" %HOOKS_LINK_FLAGS%
@echo off

cl /nologo exe2h\exe2h.c exe2h\mmap-windows.c /Fe"bin\exe2h.exe"
bin\exe2h.exe bin\hooks.x86.exe bin\hooks.x86d.exe bin\hooks.x64.exe bin\hooks.x64d.exe
move hooks_*_exe.h src
del bin\exe2h.exe exe2h.obj mmap-windows.obj bin\hooks.*.exe hooks.obj

call :VCVARSALL x86
if errorlevel 1 echo Failed to initialize x86 environment. & exit /b 1

@echo on
cl /c src\inlineexecutepe.cpp %BOF_FLAGS% /Fo"bin\inlineexecutepe.x86.o"
cl /c src\inlineexecutepe.cpp %BOF_FLAGS% /D _DEBUG /Fo"bin\inlineexecutepe.x86d.o"
cl /c src\inlinelogpe.cpp %BOF_FLAGS% /Fo"bin\inlinelogpe.x86.o"
cl /c src\inlinelogpe.cpp %BOF_FLAGS% /D _DEBUG /Fo"bin\inlinelogpe.x86d.o"
cl /c src\inlinelistpe.cpp %BOF_FLAGS% /Fo"bin\inlinelistpe.x86.o"
cl /c src\inlinelistpe.cpp %BOF_FLAGS% /D _DEBUG /Fo"bin\inlinelistpe.x86d.o"
@echo off

call :VCVARSALL x64
if errorlevel 1 echo Failed to initialize x64 environment. & exit /b 1

@echo on
cl /c src\inlineexecutepe.cpp %BOF_FLAGS% /Fo"bin\inlineexecutepe.x64.o"
cl /c src\inlineexecutepe.cpp %BOF_FLAGS% /D _DEBUG /Fo"bin\inlineexecutepe.x64d.o"
cl /c src\inlinelogpe.cpp %BOF_FLAGS% /Fo"bin\inlinelogpe.x64.o"
cl /c src\inlinelogpe.cpp %BOF_FLAGS% /D _DEBUG /Fo"bin\inlinelogpe.x64d.o"
cl /c src\inlinelistpe.cpp %BOF_FLAGS% /Fo"bin\inlinelistpe.x64.o"
cl /c src\inlinelistpe.cpp %BOF_FLAGS% /D _DEBUG /Fo"bin\inlinelistpe.x64d.o"
@echo off

strip\strip.exe ^
bin\inlineexecutepe.x86.o bin\inlineexecutepe.x86d.o bin\inlineexecutepe.x64.o bin\inlineexecutepe.x64d.o ^
bin\inlinelogpe.x86.o bin\inlinelogpe.x86d.o bin\inlinelogpe.x64.o bin\inlinelogpe.x64d.o ^
bin\inlinelistpe.x86.o bin\inlinelistpe.x86d.o bin\inlinelistpe.x64.o bin\inlinelistpe.x64d.o

copy /Y src\inline-execute-all.cna bin\inline-execute-all.cna

endlocal

exit /b 0

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
