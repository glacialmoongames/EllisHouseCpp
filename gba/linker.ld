OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
    rom (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    ewram (rw): ORIGIN = 0x02000000, LENGTH = 256K
    iwram (rw): ORIGIN = 0x03000000, LENGTH = 32K
}

SECTIONS {
    .text : { KEEP(*(.text.start)) *(.text*) *(.rodata*) . = ALIGN(4); } > rom
    .iwram : AT(LOADADDR(.text) + SIZEOF(.text)) {
        __iwram_start = .; *(.iwram*) . = ALIGN(4); __iwram_end = .;
    } > iwram
    __iwram_lma = LOADADDR(.iwram);
    .data : AT(LOADADDR(.iwram) + SIZEOF(.iwram)) {
        __data_start = .; *(.data*) . = ALIGN(4); __data_end = .;
    } > ewram
    __data_lma = LOADADDR(.data);
    .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) . = ALIGN(4); __bss_end = .; } > ewram
    /DISCARD/ : { *(.comment*) *(.ARM.attributes*) *(.note*) }
}
