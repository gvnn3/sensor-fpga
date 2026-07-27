/* 32 KB local BRAM (LMB) at 0x0 per hw/bd/static_region.tcl. */

MEMORY
{
    lmb : ORIGIN = 0x00000000, LENGTH = 0x8000
}

SECTIONS
{
    .vectors.reset 0x0 : { KEEP(*(.vectors.reset)) } > lmb
    .text 0x50 : { *(.text*) } > lmb
    .rodata : { *(.rodata*) } > lmb
    .data : { *(.data*) } > lmb
    .bss : { *(.bss*) *(COMMON) } > lmb
    _stack_top = ORIGIN(lmb) + LENGTH(lmb) - 16;
}
