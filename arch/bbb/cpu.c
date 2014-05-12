#include "cpu.h"

/*****************************************************************************
                   FUNCTION DEFINITIONS
******************************************************************************/

/*
**
** Wrapper function for the IRQ status
**
*/
unsigned int CPUIntStatus(void)
{
    unsigned int stat;

    /* IRQ and FIQ in CPSR */
    asm("    mrs     r0, CPSR\n\t"
        "    and     %[result], r0, #0xC0" : [result] "=r" (stat));
    
    return stat;
}

/*
**
** Wrapper function for the IRQ disable function
**
*/
void CPUirqd(void)
{
    /* Disable IRQ in CPSR */
    asm("    mrs     r0, CPSR\n\t"
        "    orr     r0, #0x80\n\t"
        "    msr     CPSR, r0");
}

/*
**
** Wrapper function for the IRQ enable function
**
*/
void CPUirqe(void)
{
    /* Enable IRQ in CPSR */
    asm("    mrs     r0, CPSR\n\t"
        "    bic     r0, #0x80\n\t"
        "    msr     CPSR, r0");
}

/*
**
** Wrapper function for the FIQ disable function
**
*/
void CPUfiqd(void)
{
    /* Disable FIQ in CPSR */
    asm("    mrs     r0, CPSR\n\t"
        "    orr     r0, #0x40\n\t"
        "    msr     CPSR, r0");
}

/*
**
** Wrapper function for the FIQ enable function
**
*/
void CPUfiqe(void)
{
    /* Enable FIQ in CPSR */
    asm("    mrs     r0, CPSR\n\t"
        "    bic     r0, #0x40\n\t"
        "    msr     CPSR, r0");
}

