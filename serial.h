#ifdef SERIAL_H
#error "double-included serial.h"
#endif

#define SERIAL_H

UTIL_H;
TS7200_H;

/*
 * Serial port operations.
 */

struct Port; /* opaque */
typedef volatile struct Port *Port;

#define P_TRAIN ((Port)UART1_BASE)
#define P_TTY   ((Port)UART2_BASE)

/* Enable or disable FIFOs. */
void p_enablefifo(Port p, bool enabled);

/* Set the baud rate. */
int p_setbaudrate(Port p, int baudrate);

/* Check for clear-to-send status. */
bool p_cts(Port p);

/* Attempt to write a character. True if written. */
bool p_tryputc(Port p, char c);

/* Attempt to read a character. True if read. */
bool p_trygetc(Port p, char *c_out);
