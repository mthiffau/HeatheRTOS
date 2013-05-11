#ifdef RINGBUF_H
#error "double-included ringbuf.h"
#endif

#define RINGBUF_H

UTIL_H;

/*
 * Ring buffer
 */

#define RINGBUFSIZ 1024

struct ringbuf;

/* Initialize a new ring buffer. */
void rbuf_init(struct ringbuf *r);

/* Enqueue a single character. */
int rbuf_putc(struct ringbuf *r, char c);

/* Get the first character from a ring buffer.
 * Returns true if a character was peeked at. */
bool rbuf_peekc(struct ringbuf *r, char *c_out);

/* Dequeue a single character from a ring buffer.
 * Returns true if a character was read. */
bool rbuf_getc(struct ringbuf *r, char *c_out);

/* Write many characters to ring buffer. */
int rbuf_write(struct ringbuf *r, int n, const char *s);

/* Print a NUL-terminated string verbatim. */
int rbuf_print(struct ringbuf *r, const char *s);

/* Struct definition */
struct ringbuf {
    int  rd, wr, len;
    char buf[RINGBUFSIZ];
};
