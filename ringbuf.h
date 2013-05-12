#ifdef RINGBUF_H
#error "double-included ringbuf.h"
#endif

#define RINGBUF_H

XBOOL_H;
XDEF_H;
XARG_H;

/*
 * Ring buffer
 */

struct ringbuf;

/* Initialize a new ring buffer. */
void rbuf_init(struct ringbuf *r, char *mem, size_t size);

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

/* Repeat a character */
int rbuf_nputc(struct ringbuf *r, int n, char ch);

/* Print aligned strings */
int rbuf_alignl(struct ringbuf *r, int w, char pad, const char *s);
int rbuf_alignr(struct ringbuf *r, int w, char pad, const char *s);

/* Formatted printing. Returns 0 for success, unlike real printf! */
int rbuf_vprintf(struct ringbuf *r, char *fmt, va_list args);
int rbuf_printf(struct ringbuf *r, char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* Struct definition */
struct ringbuf {
    char  *mem;
    size_t size;
    size_t rd;
    size_t wr;
    size_t len;
};
