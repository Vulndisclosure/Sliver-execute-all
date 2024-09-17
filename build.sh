#!/usr/bin/env sh

CC32="i686-w64-mingw32-gcc"
CC64="x86_64-w64-mingw32-gcc"
CC32="$CC32 -msse2"
CC64="$CC64 -mcmodel=small"
WARNING_FLAGS="-Wall -Wextra -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-unused-function -Wno-unused-value -Wno-array-bounds"
COMMON_FLAGS="-Os -std=c++17 $WARNING_FLAGS -D JM_XORSTR_DISABLE_AVX_INTRINSICS"
HOOKS_FLAGS="$COMMON_FLAGS -fno-toplevel-reorder -nostdlib"
BOF_FLAGS="$COMMON_FLAGS -D BOF"

$CC32 src/hooks.cpp src/hooks.x86.obj $COMMON_FLAGS $HOOKS_FLAGS -o bin/hooks.x86.exe &
$CC64 src/hooks.cpp src/hooks.x64.obj $COMMON_FLAGS $HOOKS_FLAGS -o bin/hooks.x64.exe &
$CC32 src/hooks.cpp src/hooks.x86.obj $COMMON_FLAGS $HOOKS_FLAGS -D _DEBUG -o bin/hooks.x86d.exe &
$CC64 src/hooks.cpp src/hooks.x64.obj $COMMON_FLAGS $HOOKS_FLAGS -D _DEBUG -o bin/hooks.x64d.exe &
wait

gcc exe2h/exe2h.c -obin/exe2h.out
./bin/exe2h.out bin/hooks.x86.exe bin/hooks.x86d.exe bin/hooks.x64.exe bin/hooks.x64d.exe
mv hooks_*_exe.h src
rm bin/exe2h.out bin/hooks.*.exe

$CC32 -c src/inlineexecutepe.cpp $BOF_FLAGS -o bin/inlineexecutepe.x86.o &
$CC64 -c src/inlineexecutepe.cpp $BOF_FLAGS -o bin/inlineexecutepe.x64.o &
$CC32 -c src/inlineexecutepe.cpp $BOF_FLAGS -D _DEBUG -o bin/inlineexecutepe.x86d.o &
$CC64 -c src/inlineexecutepe.cpp $BOF_FLAGS -D _DEBUG -o bin/inlineexecutepe.x64d.o &
$CC32 -c src/inlinelogpe.cpp $BOF_FLAGS -o bin/inlinelogpe.x86.o &
$CC64 -c src/inlinelogpe.cpp $BOF_FLAGS -o bin/inlinelogpe.x64.o &
$CC32 -c src/inlinelogpe.cpp $BOF_FLAGS -D _DEBUG -o bin/inlinelogpe.x86d.o &
$CC64 -c src/inlinelogpe.cpp $BOF_FLAGS -D _DEBUG -o bin/inlinelogpe.x64d.o &
$CC32 -c src/inlinelistpe.cpp $BOF_FLAGS -o bin/inlinelistpe.x86.o &
$CC64 -c src/inlinelistpe.cpp $BOF_FLAGS -o bin/inlinelistpe.x64.o &
$CC32 -c src/inlinelistpe.cpp $BOF_FLAGS -D _DEBUG -o bin/inlinelistpe.x86d.o &
$CC64 -c src/inlinelistpe.cpp $BOF_FLAGS -D _DEBUG -o bin/inlinelistpe.x64d.o &
wait

cp -f src/inline-execute-all.cna bin/inline-execute-all.cna
