#include "xbool.h"
#include "xdef.h"
#include "xarg.h"
#include "ringbuf.h"

#include "bwio.h" /* for debug print */

/* FIXME HACK */
void bwui2a(unsigned int num, unsigned int base, char *bf);
void bwi2a(int num, char *bf);

void rbuf_init(struct ringbuf *r, char *mem, size_t size)
{
    r->mem  = mem;
    r->size = size;
    r->rd   = 0;
    r->wr   = 0;
    r->len  = 0;
}

void rbuf_putc(struct ringbuf *r, char c)
{
    /* FIXME */
    if (r->len == r->size) {
        bwprintf(COM2, "\e[20;1f buffer full!");
        for (;;) { /* assertion failure */ }
    }

    r->mem[r->wr++] = c;
    r->wr %= r->size;
    r->len++;
}

bool rbuf_peekc(struct ringbuf *r, char *c_out)
{
    if (r->len == 0)
        return false;

    *c_out = r->mem[r->rd];
    return true;
}

bool rbuf_getc(struct ringbuf *r, char *c_out)
{
    if (r->len == 0)
        return false;

    *c_out = r->mem[r->rd++];
    r->rd %= r->size;
    r->len--;
    return true;
}

void rbuf_write(struct ringbuf *r, const void *buf, int n)
{
    int i;
    const char *s = buf;
    for (i = 0; i < n; i++)
        rbuf_putc(r, *s++);
}

void rbuf_print(struct ringbuf *r, const char *s)
{
    char c;
    while ((c = *s++))
        rbuf_putc(r, c);
}

void rbuf_printf(struct ringbuf *r, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    rbuf_vprintf(r, fmt, args);
    va_end(args);
}

void rbuf_nputc(struct ringbuf *r, int n, char ch)
{
    while (n-- > 0)
        rbuf_putc(r, ch);
}

void rbuf_dec(struct ringbuf *r, int n)
{
    rbuf_decw(r, n, 0, ' ');
}

void rbuf_decw(struct ringbuf *r, int n, int width, char pad)
{
    char ds[11]; /* no NUL-terminator */
    int  len = 0;

    if (n == 0) {
        ds[len++] = '0';
    } else {
        bool neg = n < 0;
        if (neg)
            n = -n;
        while (n > 0) {
            ds[len++] = '0' + n % 10;
            n /= 10;
        }
        if (neg)
            ds[len++] = '-';
    }

    if (width > len)
        rbuf_nputc(r, width - len, pad);
    while (len > 0)
        rbuf_putc(r, ds[--len]);
}

static void rbuf_align(
    struct ringbuf *r,
    int w,
    char pad,
    bool left,
    const char *buf)
{
    const char *p = buf;
    while (*p++ && w > 0)
        w--;
    if (!left)
        rbuf_nputc(r, w, pad);
    rbuf_print(r, buf);
    if (left)
        rbuf_nputc(r, w, pad);
}

void rbuf_alignl(struct ringbuf *r, int w, char pad, const char *s)
{
    rbuf_align(r, w, pad, true, s);
}

void rbuf_alignr(struct ringbuf *r, int w, char pad, const char *s)
{
    rbuf_align(r, w, pad, false, s);
}

/* Much the same as bwformat() */
void rbuf_vprintf(struct ringbuf *r, const char *fmt, va_list args)
{
    char ch, pad, buf[16];
    int  w;
    bool left;

    while ((ch = *fmt++)) {
        if (ch != '%') {
            rbuf_putc(r, ch);
            continue;
        }

        /* Got a % */
        ch   = *fmt++;
        pad  = '\0';
        w    = 0;
        left = false;

        if (ch == '-') {
            left = true;
            ch = *fmt++;
        }

        if (ch >= '1' && ch <= '9') {
            pad = ' ';
        } else if (ch == '0') {
            ch = *fmt++;
            if (!left)
                pad = '0';
        }

        while (ch >= '0' && ch <= '9') {
            w *= 10;
            w += ch - '0';
            ch = *fmt++;
        }

        switch (ch) {
        case 'c':
            buf[0] = va_arg(args, char);
            buf[1] = '\0';
            rbuf_align(r, w, pad, left, buf);
            break;

        case 's':
            rbuf_align(r, w, pad, left, va_arg(args, char*));
            break;

        case 'u':
            bwui2a(va_arg(args, unsigned), 10, buf);
            rbuf_align(r, w, pad, left, buf);
            break;

        case 'd':
            bwi2a(va_arg(args, int), buf);
            rbuf_align(r, w, pad, left, buf);
            break;

        case 'x':
            bwui2a(va_arg(args, unsigned), 16, buf);
            rbuf_align(r, w, pad, left, buf);
            break;

        case 'p':
            buf[0] = '0';
            buf[1] = 'x';
            bwui2a(va_arg(args, unsigned), 16, buf + 2);
            rbuf_align(r, w, pad, left, buf);
            break;

        case '%':
            buf[0] = '%';
            buf[1] = '\0';
            rbuf_align(r, w, pad, left, buf);
            break;
        }
    }
}
