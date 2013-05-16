ENTRY (main)

/*
MEMORY
{
    ram : ORIGIN = 0x00000000, LENGTH = 32M
    rom : ORIGIN = 0x60000000, LENGTH = 8M
}
*/

SECTIONS
{
    . = 0x00218000;

    .text : /* The actual instructions. */
    {
        *(.text)
        *(.got)
        *(.got.plt)
        *(.rodata)
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
}
