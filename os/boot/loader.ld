OUTPUT_ARCH(riscv)
ENTRY(_boot_start)

MEMORY
{
    boot_ram (rwx) : ORIGIN = 0x80000000, LENGTH = 2M
}

SECTIONS
{
    . = ORIGIN(boot_ram);

    .text : {
        *(.text.init)
        *(.text)
    } > boot_ram

    .rodata : {
        *(.rodata)
        *(.srodata)
    } > boot_ram

    .data : {
        *(.data)
        *(.sdata)
    } > boot_ram

    .bss (NOLOAD) : {
        . = ALIGN(8);
        _boot_bss_start = .;
        *(.bss)
        *(.sbss)
        *(COMMON)
        . = ALIGN(8);
        _boot_bss_end = .;
    } > boot_ram

    _boot_end = .;
    _boot_stack_top = ORIGIN(boot_ram) + LENGTH(boot_ram);

    ASSERT(_boot_end < _boot_stack_top - 0x1000,
           "bootloader overlaps its stack")
}
