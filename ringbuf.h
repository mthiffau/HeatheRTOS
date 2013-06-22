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
void rbuf_putc(struct ringbuf *r, char c);

/* Get the first character from a ring buffer.
 * Returns true if a character was peeked at. */
bool rbuf_peekc(struct ringbuf *r, char *c_out);

/* Dequeue a single character from a ring buffer.
 * Returns true if a character was read. */
bool rbuf_getc(struct ringbuf *r, char *c_out);

/* Write many characters to ring buffer. */
void rbuf_write(struct ringbuf *r, const void *buf, int n);

/* Print a NUL-terminated string verbatim. */
void rbuf_print(struct ringbuf *r, const char *s);

/* Repeat a character */
void rbuf_nputc(struct ringbuf *r, int n, char ch);

/* Print an signed integer in decimal */
void rbuf_dec(struct ringbuf *r, int n);
void rbuf_decw(struct ringbuf *r, int n, int width, char pad);

/* Print aligned strings */
void rbuf_alignl(struct ringbuf *r, int w, char pad, const char *s);
void rbuf_alignr(struct ringbuf *r, int w, char pad, const char *s);

/* Formatted printing. Returns 0 for success, unlike real printf! */
void rbuf_vprintf(struct ringbuf *r, const char *fmt, va_list args);
void rbuf_printf(struct ringbuf *r, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* Struct definition */
struct ringbuf {
    char  *mem;
    size_t size;
    size_t rd;
    size_t wr;
    size_t len;
};
