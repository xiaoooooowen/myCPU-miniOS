OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    ram (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
    . = ORIGIN(ram);

    _kernel_start = .;

    .text : {
        *(.text.init)
        *(.text)
    } > ram

    .rodata : {
        *(.rodata)
    } > ram

    .data : {
        *(.data)
    } > ram

    .bss : {
        _bss_start = .;
        *(.bss)
        *(COMMON)
        _bss_end = .;
    } > ram

    _kernel_end = .;

    . = ALIGN(16);
    _stack_top = ORIGIN(ram) + LENGTH(ram);
}
