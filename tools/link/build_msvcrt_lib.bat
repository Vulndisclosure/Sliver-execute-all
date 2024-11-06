@echo off
REM Copy WinDDK\7600.16385.1\lib\Crt\i386\msvcrt.lib -> msvcrt.x86.lib
REM Copy WinDDK\7600.16385.1\lib\Crt\amd64\msvcrt.lib -> msvcrt.x64.lib
if not exist "msvcrt.x86.lib.bak" copy "msvcrt.x86.lib" "msvcrt.x86.lib.bak"
if not exist "msvcrt.x64.lib.bak" copy "msvcrt.x64.lib" "msvcrt.x64.lib.bak"
llvm-ar -t msvcrt.x86.lib | findstr /V /C:msvcrt.dll > temp_objs.txt
llvm-ar d msvcrt.x86.lib @temp_objs.txt
llvm-ar -t msvcrt.x64.lib | findstr /V /C:msvcrt.dll > temp_objs.txt
llvm-ar d msvcrt.x64.lib @temp_objs.txt
del temp_objs.txt
