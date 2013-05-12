#ifdef SERIAL_H
#error "double-included serial.h"
#endif

#define SERIAL_H

XBOOL_H;
XINT_H;
XDEF_H;
TS7200_H;
STATIC_ASSERT_H;

/*
 * Serial port operations.
 */

struct uart;
typedef volatile struct uart *Port;

#define P_TRAIN ((Port)UART1_BASE)
#define P_TTY   ((Port)UART2_BASE)

/* Enable or disable FIFOs. */
void p_enable_fifo(Port p, bool enabled);

/* Set the baud rate. */
int p_set_baudrate(Port p, int baudrate);

/* Enable or disable parity. */
void p_enable_parity(Port p, bool enabled);

/* Set parity to even or odd. */
#define PARITY_EVEN true
#define PARITY_ODD  false
void p_select_parity(Port p, bool even);

/* Enable or disable sending 2 stop bits instead of 1. */
void p_enable_2stop(Port p, bool enable);

/* Check for clear-to-send status. */
bool p_cts(Port p);

/* Attempt to write a character. True if written. */
bool p_tryputc(Port p, char c);

/* Attempt to read a character. True if read. */
bool p_trygetc(Port p, char *c_out);

/* Struct definition */
struct uart {
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

STATIC_ASSERT(data_offs, offsetof (struct uart, data) == UART_DATA_OFFSET);
STATIC_ASSERT(rsr_offs,  offsetof (struct uart, rsr)  == UART_RSR_OFFSET);
STATIC_ASSERT(lcrh_offs, offsetof (struct uart, lcrh) == UART_LCRH_OFFSET);
STATIC_ASSERT(lcrm_offs, offsetof (struct uart, lcrm) == UART_LCRM_OFFSET);
STATIC_ASSERT(lcrl_offs, offsetof (struct uart, lcrl) == UART_LCRL_OFFSET);
STATIC_ASSERT(ctrl_offs, offsetof (struct uart, ctrl) == UART_CTLR_OFFSET);
STATIC_ASSERT(flag_offs, offsetof (struct uart, flag) == UART_FLAG_OFFSET);
STATIC_ASSERT(intr_offs, offsetof (struct uart, intr) == UART_INTR_OFFSET);
STATIC_ASSERT(dmar_offs, offsetof (struct uart, dmar) == UART_DMAR_OFFSET);
