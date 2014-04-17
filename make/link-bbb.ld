ENTRY (_asm_entry)

SECTIONS
{
    . = 0x80200000;
    . = ALIGN(4);
    .text : /* The actual instructions. */
    {
        build/arch/bbb/startup.S.o(.text)
        *(.text)
        *(.got)
        *(.got.plt)
        *(.rodata)
        *(.rodata.*)
        *(.glue_7)
        *(.glue_7t)
    }

    . = ALIGN(4);
    .data : /* Initialized data. */
    {
        _DataStart = . ;
        *(.data)
        _DataEnd = . ;
    }

    . = ALIGN(4);
    .bss : /* Uninitialized data. */
    {
        _BssStart = . ;
        *(.bss)
        _BssEnd = . ;
    }

    . = ALIGN(8);
    _ProgramEnd = . ;

    . = 0x9FFFFFF0;
    _MemoryEnd = . ;
}
