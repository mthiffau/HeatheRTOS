ENTRY (_asm_entry)

SECTIONS
{
    . = 0x40300000;
    _OCMCStart = . ;

    . = ALIGN(4);
    .text : /* The actual instructions. */
    {
	_TextStart = . ;
        build/arch/bbb/startup.S.o(.text)
        *(.text)
        *(.got)
        *(.got.plt)
        *(.rodata)
        *(.rodata.*)
        *(.glue_7)
        *(.glue_7t)
	_TextEnd = . ;
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

    /* Section of memory for kernel stack */
    . = ALIGN(4);
    _KernStackTop = . ;

    . = 0x4030FFF0;
    _KernStackBottom = . ;

    . = 0x4030FFFF;
    _OCMCEnd = . ;

    . = 0x80000000;
    _DDRStart = . ;

    /* Section of memory for user stacks */
    . = ALIGN(8);
    _UserStacksStart = . ;

    . = 0x9FFFFFF0;
    _UserStacksEnd = . ;

    . = 0x9FFFFFFF;
    _DDREnd = . ;
}
