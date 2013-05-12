#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "ts7200.h"
#include "static_assert.h"
#include "serial.h"

void p_enable_fifo(Port p, bool enabled)
{
    if (enabled)
        p->lcrh |= FEN_MASK;
    else
        p->lcrh &= ~FEN_MASK;
}

int p_set_baudrate(Port p, int baudrate)
{
    switch (baudrate) {
    case 115200:
        p->lcrm = 0x0;
        p->lcrl = 0x3;
        return 0;
    case 2400:
        p->lcrm = 0x0;
        p->lcrl = 0xbf;
        return 0;
    default:
        return -1;
    }
}

void p_enable_parity(Port p, bool enable)
{
    if (enable)
        p->lcrh |= PEN_MASK;
    else
        p->lcrh &= ~PEN_MASK;
}

void p_select_parity(Port p, bool even)
{
    if (even)
        p->lcrh |= EPS_MASK;
    else
        p->lcrh &= ~EPS_MASK;
}

void p_enable_2stop(Port p, bool enable)
{
    if (enable)
        p->lcrh |= STP2_MASK;
    else
        p->lcrh &= ~STP2_MASK;
}

bool p_cts(Port p)
{
    return !(p->flag & CTS_MASK);
}

bool p_tryputc(Port p, char c)
{
    uint32_t flag = p->flag;
    if ((flag & TXFF_MASK) || (flag & TXBUSY_MASK))
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
