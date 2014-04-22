/* Code to set up exception vectors for beaglebone black */

#include "config.h"
#include "xint.h"
#include "xarg.h"
#include "xdef.h"
#include "static_assert.h"
#include "bwio.h"

#include "u_tid.h"
#include "task.h"

#include "cp15.h"
#include "ctx_switch.h"
#include "exc_vec.h"

/* This is where we're going to put the exception vector table */
const unsigned int AM335X_VECTOR_BASE = 0x4030FC00;

/* Place holders for things we don't handle yet... */
static void entry(void);
static void undef_inst_handler(void);
static void abort_handler(void);
static void FIQ_handler(void);

/* These are the things going into the exception vector table */
static unsigned int const vecTbl[14] = 
{
    0xE59FF018, // Opcode for loading PC with the contents of [PC + 0x18]
    0xE59FF018, // Opcode for loading PC with the contents of [PC + 0x18]
    0xE59FF018, // Opcode for loading PC with the contents of [PC + 0x18]
    0xE59FF018, // Opcode for loading PC with the contents of [PC + 0x18]
    0xE59FF014, // Opcode for loading PC with the contents of [PC + 0x14]
    0xE24FF008, // Opcode for loading PC with (PC - 8) (eq. to while(1))
    0xE59FF010, // Opcode for loading PC with the contents of [PC + 0x10]
    0xE59FF010, // Opcode for loading PC with the contents of [PC + 0x10]
    (unsigned int)entry,
    (unsigned int)undef_inst_handler,
    (unsigned int)kern_entry_swi,
    (unsigned int)abort_handler,
    (unsigned int)kern_entry_irq,
    (unsigned int)FIQ_handler
};

/* Load vector table into memory */
void 
load_vector_table(void)
{
    unsigned int *dest = (unsigned int *)AM335X_VECTOR_BASE;
    unsigned int *src = (unsigned int *)vecTbl;
    unsigned int count;
  
    CP15VectorBaseAddrSet(AM335X_VECTOR_BASE);

    for(count = 0; count < sizeof(vecTbl)/sizeof(vecTbl[0]); count++)
    {
	dest[count] = src[count];
    }
}

static void
entry(void)
{
    bwputstr("Whatever entry is...this is it.\n\r");
    while(1);
}

static void
undef_inst_handler(void)
{
    bwputstr("Undefined instruction! Luck you...\n\r");
    while(1);
}

static void
abort_handler(void)
{
    bwputstr("Something you did caused a data abort...\n\r");
    while(1);
}

static void
FIQ_handler(void)
{
    bwputstr("FIQ's are not supported on this system!\n\r");
    while(1);
}
