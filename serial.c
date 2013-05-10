#include "util.h"
#include "serial.h"

#include "ts7200.h"
#include "bwio.h"   /* bwsetfifo, bwsetspeed */

struct ts72uart {
    uint32_t data;
    uint32_t rsr;
    uint32_t lcrh;
    uint32_t lcrm;
    uint32_t lcrl;
    uint32_t ctrl;
    uint32_t flag;
    uint32_t intr;
    uint32_t reserved[2];
    uint32_t dmar;
};

STATIC_ASSERT(data_offs, offsetof (struct ts72uart, data) == UART_DATA_OFFSET);
STATIC_ASSERT(rsr_offs,  offsetof (struct ts72uart, rsr)  == UART_RSR_OFFSET);
STATIC_ASSERT(lcrh_offs, offsetof (struct ts72uart, lcrh) == UART_LCRH_OFFSET);
STATIC_ASSERT(lcrm_offs, offsetof (struct ts72uart, lcrm) == UART_LCRM_OFFSET);
STATIC_ASSERT(lcrl_offs, offsetof (struct ts72uart, lcrl) == UART_LCRL_OFFSET);
STATIC_ASSERT(ctrl_offs, offsetof (struct ts72uart, ctrl) == UART_CTLR_OFFSET);
STATIC_ASSERT(flag_offs, offsetof (struct ts72uart, flag) == UART_FLAG_OFFSET);
STATIC_ASSERT(intr_offs, offsetof (struct ts72uart, intr) == UART_INTR_OFFSET);
STATIC_ASSERT(dmar_offs, offsetof (struct ts72uart, dmar) == UART_DMAR_OFFSET);

/* FIXME I want to put these in a static array,
 * but it always returns NULL if I do. */
static inline volatile struct ts72uart *p_uart(Port p)
{
    switch (unPort(p)) {
        case P_COM1_: return (volatile struct ts72uart*)UART1_BASE;
        case P_COM2_: return (volatile struct ts72uart*)UART2_BASE;
    }
    return (volatile struct ts72uart*)0;
}

int p_enablefifo(Port p, bool enabled)
{
    return bwsetfifo(unPort(p), enabled);
}

int p_setbaudrate(Port p, int baudrate)
{
    return bwsetspeed(unPort(p), baudrate);
}

bool p_tryputc(Port p, char c)
{
    volatile struct ts72uart *uart = p_uart(p);
    if (uart->flag & TXFF_MASK)
        return false;

    uart->data = c;
    return true;
}

bool p_trygetc(Port p, char *c_out)
{
    volatile struct ts72uart *uart = p_uart(p);
    if (!(uart->flag & RXFF_MASK))
        return false;

    *c_out = uart->data;
    return true;
}
