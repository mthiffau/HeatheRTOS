#include "util.h"
#include "ts7200.h"
#include "serial.h"

struct Port {
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

STATIC_ASSERT(data_offs, offsetof (struct Port, data) == UART_DATA_OFFSET);
STATIC_ASSERT(rsr_offs,  offsetof (struct Port, rsr)  == UART_RSR_OFFSET);
STATIC_ASSERT(lcrh_offs, offsetof (struct Port, lcrh) == UART_LCRH_OFFSET);
STATIC_ASSERT(lcrm_offs, offsetof (struct Port, lcrm) == UART_LCRM_OFFSET);
STATIC_ASSERT(lcrl_offs, offsetof (struct Port, lcrl) == UART_LCRL_OFFSET);
STATIC_ASSERT(ctrl_offs, offsetof (struct Port, ctrl) == UART_CTLR_OFFSET);
STATIC_ASSERT(flag_offs, offsetof (struct Port, flag) == UART_FLAG_OFFSET);
STATIC_ASSERT(intr_offs, offsetof (struct Port, intr) == UART_INTR_OFFSET);
STATIC_ASSERT(dmar_offs, offsetof (struct Port, dmar) == UART_DMAR_OFFSET);

void p_enablefifo(Port p, bool enabled)
{
    if (enabled)
        p->lcrh |= FEN_MASK;
    else
        p->lcrh &= ~FEN_MASK;
}

int p_setbaudrate(Port p, int baudrate)
{
    switch (baudrate) {
    case 115200:
        p->lcrm = 0x0;
        p->lcrl = 0x3;
        return 0;
    case 2400:
        p->lcrm = 0x0;
        p->lcrl = 0x90;
        return 0;
    default:
        return -1;
    }
}

bool p_cts(Port p)
{
    return !(p->flag & CTS_MASK);
}

bool p_tryputc(Port p, char c)
{
    if (p->flag & TXFF_MASK)
        return false;

    p->data = c;
    return true;
}

bool p_trygetc(Port p, char *c_out)
{
    if (!(p->flag & RXFF_MASK))
        return false;

    *c_out = p->data;
    return true;
}
