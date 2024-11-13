#!/usr/bin/env sh

main() {
CC32="i686-w64-mingw32-gcc"
CC64="x86_64-w64-mingw32-gcc"
CC32="$CC32 -msse2"
CC64="$CC64 -mcmodel=small"
WARNING_FLAGS="-Wall -Wextra -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-unused-function -Wno-unused-value -Wno-unused-variable -Wno-array-bounds"
CC_FLAGS="-Os -fomit-frame-pointer -fno-ident -nostdlib -ffunction-sections -fdata-sections -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables $WARNING_FLAGS -D __USE_MINGW_ANSI_STDIO=0"
BOF_FLAGS="$CC_FLAGS -fno-function-sections -fno-data-sections -D BOF"
C2SC_LIB_FLAGS="-lkernel32"
C2SC_LINK_FLAGS="$C2SC_LIB_FLAGS -Ltools/link -lgcc -Wl,--gc-sections,--section-alignment,16,--file-alignment,16,--image-base,0,-T,linker.ld"
C2SC_LINK_FLAGS32="-l:msvcrt.x86.lib $C2SC_LINK_FLAGS"
C2SC_LINK_FLAGS64="-l:msvcrt.x64.lib $C2SC_LINK_FLAGS"

mkdir -p bin

$CC32 src/hooks/hooks.cc src/hooks/control.cc src/hooks/log.cc src/hooks/pipeserver.cc src/hooks/simplelist1.cc src/hooks/simplelist2.cc $CC_FLAGS -o bin/hooks.x86.exe $C2SC_LINK_FLAGS32,-e,_HookControlEntry &
$CC64 src/hooks/hooks.cc src/hooks/control.cc src/hooks/log.cc src/hooks/pipeserver.cc src/hooks/simplelist1.cc src/hooks/simplelist2.cc $CC_FLAGS -o bin/hooks.x64.exe $C2SC_LINK_FLAGS64,-e,HookControlEntry &
$CC32 src/hooks/hooks.cc src/hooks/control.cc src/hooks/log.cc src/hooks/pipeserver.cc src/hooks/simplelist1.cc src/hooks/simplelist2.cc $CC_FLAGS -D _DEBUG -o bin/hooks.x86d.exe $C2SC_LINK_FLAGS32,-e,_HookControlEntry &
$CC64 src/hooks/hooks.cc src/hooks/control.cc src/hooks/log.cc src/hooks/pipeserver.cc src/hooks/simplelist1.cc src/hooks/simplelist2.cc $CC_FLAGS -D _DEBUG -o bin/hooks.x64d.exe $C2SC_LINK_FLAGS64,-e,HookControlEntry &
$CC32 src/c2scstartup.cc src/fileserver.cc src/globalshareddata.cc $CC_FLAGS -o bin/fileserver.x86.exe $C2SC_LINK_FLAGS32,-e,_Entry &
$CC64 src/c2scstartup.cc src/fileserver.cc src/globalshareddata.cc $CC_FLAGS -o bin/fileserver.x64.exe $C2SC_LINK_FLAGS64,-e,Entry &
$CC32 src/c2scstartup.cc src/fileserver.cc src/globalshareddata.cc $CC_FLAGS -D _DEBUG -o bin/fileserver.x86d.exe $C2SC_LINK_FLAGS32,-e,_Entry &
$CC64 src/c2scstartup.cc src/fileserver.cc src/globalshareddata.cc $CC_FLAGS -D _DEBUG -o bin/fileserver.x64d.exe $C2SC_LINK_FLAGS64,-e,Entry &
wait

gcc tools/c2sc.c -otools/c2sc
./tools/c2sc hs bin/hooks.x86.exe bin/hooks.x86d.exe bin/hooks.x64.exe bin/hooks.x64d.exe
mv hooks_*.h src/hooks
./tools/c2sc b bin/fileserver.x86.exe bin/fileserver.x86d.exe bin/fileserver.x64.exe bin/fileserver.x64d.exe
mv fileserver.*.bin bin
rm tools/c2sc bin/hooks.*.exe bin/fileserver.*.exe

$CC32 -c src/inlineexecutepe.cc $BOF_FLAGS -o bin/inlineexecutepe.x86.o &
$CC64 -c src/inlineexecutepe.cc $BOF_FLAGS -o bin/inlineexecutepe.x64.o &
$CC32 -c src/inlineexecutepe.cc $BOF_FLAGS -D _DEBUG -o bin/inlineexecutepe.x86d.o &
$CC64 -c src/inlineexecutepe.cc $BOF_FLAGS -D _DEBUG -o bin/inlineexecutepe.x64d.o &
$CC32 -c src/inlinelogpe.cc $BOF_FLAGS -o bin/inlinelogpe.x86.o &
$CC64 -c src/inlinelogpe.cc $BOF_FLAGS -o bin/inlinelogpe.x64.o &
$CC32 -c src/inlinelogpe.cc $BOF_FLAGS -D _DEBUG -o bin/inlinelogpe.x86d.o &
$CC64 -c src/inlinelogpe.cc $BOF_FLAGS -D _DEBUG -o bin/inlinelogpe.x64d.o &
$CC32 -c src/inlinelistpe.cc $BOF_FLAGS -o bin/inlinelistpe.x86.o &
$CC64 -c src/inlinelistpe.cc $BOF_FLAGS -o bin/inlinelistpe.x64.o &
$CC32 -c src/inlinelistpe.cc $BOF_FLAGS -D _DEBUG -o bin/inlinelistpe.x86d.o &
$CC64 -c src/inlinelistpe.cc $BOF_FLAGS -D _DEBUG -o bin/inlinelistpe.x64d.o &
wait

g++ -O2 -std=c++20 tools/fixbof.cc tools/obj.cc -lfmt -otools/fixbof
./tools/fixbof tools/link \
bin/inlineexecutepe.x86.o bin/inlineexecutepe.x86d.o bin/inlineexecutepe.x64.o bin/inlineexecutepe.x64d.o \
bin/inlinelogpe.x86.o bin/inlinelogpe.x86d.o bin/inlinelogpe.x64.o bin/inlinelogpe.x64d.o \
bin/inlinelistpe.x86.o bin/inlinelistpe.x86d.o bin/inlinelistpe.x64.o bin/inlinelistpe.x64d.o
rm tools/fixbof

cp -f src/inline-execute-all.cna bin/inline-execute-all.cna
}

create_linker_ld() {
cat << 'EOF' > linker.ld
SECTIONS
{
  /* Make the virtual address and file offset synced if the alignment is
     lower than the target page size. */
  . = SIZEOF_HEADERS;
  . = ALIGN(__section_alignment__);
  .text  __image_base__ + ( __section_alignment__ < 0x1000 ? . : __section_alignment__ ) :
  {
    /* .text */
    *(.text.entry)
    *(.text)
    *(SORT(.text$*))
    *(.text.*)
    *(.gnu.linkonce.t.*)
	/* IAT */
    __IAT_start__ = .;
    KEEP (SORT(*)(.idata$5))
    __IAT_end__ = .;
	/* .rdata */
	*(.rdata)
    *(SORT(.rdata$*))
	/* .data */
    __data_start__ = . ;
    *(.data)
    *(.data2)
    *(SORT(.data$*))
    __data_end__ = . ;
  }
  .idata BLOCK(__section_alignment__) :
  {
    /* This cannot currently be handled with grouped sections.
        See pe.em:sort_sections.  */
    KEEP (SORT(*)(.idata$2))
    KEEP (SORT(*)(.idata$3))
    /* These zeroes mark the end of the import list.  */
    LONG (0); LONG (0); LONG (0); LONG (0); LONG (0);
    KEEP (SORT(*)(.idata$4))
    KEEP (SORT(*)(.idata$6))
    KEEP (SORT(*)(.idata$7))
  }
  .eh_frame BLOCK(__section_alignment__) :
  {
    KEEP (*(.eh_frame*))
  }
  .pdata BLOCK(__section_alignment__) :
  {
    KEEP(*(.pdata*))
  }
  .xdata BLOCK(__section_alignment__) :
  {
    KEEP(*(.xdata*))
  }
  .bss BLOCK(__section_alignment__) :
  {
    __bss_start__ = . ;
    *(.bss)
    *(COMMON)
    __bss_end__ = . ;
  }
  .edata BLOCK(__section_alignment__) :
  {
    *(.edata)
  }
  /DISCARD/ :
  {
    *(.debug$S)
    *(.debug$T)
    *(.debug$F)
     *(.drectve)
     *(.note.GNU-stack)
     *(.gnu.lto_*)
  }
  .CRT BLOCK(__section_alignment__) :
  {
    ___crt_xc_start__ = . ;
    KEEP (*(SORT(.CRT$XC*)))  /* C initialization */
    ___crt_xc_end__ = . ;
    ___crt_xi_start__ = . ;
    KEEP (*(SORT(.CRT$XI*)))  /* C++ initialization */
    ___crt_xi_end__ = . ;
    ___crt_xl_start__ = . ;
    KEEP (*(SORT(.CRT$XL*)))  /* TLS callbacks */
    /* ___crt_xl_end__ is defined in the TLS Directory support code */
    ___crt_xp_start__ = . ;
    KEEP (*(SORT(.CRT$XP*)))  /* Pre-termination */
    ___crt_xp_end__ = . ;
    ___crt_xt_start__ = . ;
    KEEP (*(SORT(.CRT$XT*)))  /* Termination */
    ___crt_xt_end__ = . ;

    KEEP (*(SORT_NONE(.init)))
       KEEP (*(.ctors));
       KEEP (*(.ctor));
       KEEP (*(SORT_BY_NAME(.ctors.*)));
       KEEP (*(.dtors));
       KEEP (*(.dtor));
       KEEP (*(SORT_BY_NAME(.dtors.*)));
    KEEP (*(SORT_NONE(.fini)))
  }
  .rsrc BLOCK(__section_alignment__) : SUBALIGN(4)
  {
    KEEP (*(.rsrc))
    KEEP (*(.rsrc$*))
  }
  .reloc BLOCK(__section_alignment__) :
  {
    *(.reloc)
  }
}
EOF
}

create_linker_ld
main
rm linker.ld