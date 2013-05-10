#ifdef SERIAL_H
#error "double-included serial.h"
#endif

#define SERIAL_H

UTIL_H;

/*
 * Available serial ports
 */

enum Port_ {
    P_COM1_ = 0,
    P_COM2_ = 1,
};

NEWTYPE(Port, enum Port_);
#define P_COM1       NTLIT(Port, P_COM1_) /* Typed constants */
#define P_COM2       NTLIT(Port, P_COM2_)

#define P_TRAIN      P_COM1               /* Semantic names */
#define P_TTY        P_COM2


/*
 * Serial port operations.
 */

/* Enable or disable FIFOs. */
int p_enablefifo(Port p, bool enabled);

/* Set the baud rate. */
int p_setbaudrate(Port p, int baudrate);

/* Check for clear-to-send status. */
bool p_cts(Port p);

/* Attempt to write a character. True if written. */
bool p_tryputc(Port p, char c);

/* Attempt to read a character. True if read. */
bool p_trygetc(Port p, char *c_out);
