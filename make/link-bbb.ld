ENTRY (_asm_entry)

SECTIONS
{
    . = 0x402F0400;

    .text : /* The actual instructions. */
    {
        build/startup.S.o(.text)
        *(.text)
        *(.got)
        *(.got.plt)
        *(.rodata)
        *(.rodata.*)
        *(.glue_7)
        *(.glue_7t)
    }

    .data : /* Initialized data. */
    {
        _DataStart = . ;
        *(.data)
        _DataEnd = . ;
    }

    .bss : /* Uninitialized data. */
    {
        _BssStart = . ;
        *(.bss)
        _BssEnd = . ;
    }

    . = ALIGN(8);
    _ProgramEnd = . ;

    . = 0x402FFFF0;
    _MemoryEnd = . ;
}
