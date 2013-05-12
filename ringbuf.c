#include "xbool.h"
#include "xdef.h"
#include "xarg.h"
#include "ringbuf.h"

#include "bwio.h" /* for debug print */

/* FIXME HACK */
void bwui2a(unsigned int num, unsigned int base, char *bf);
void bwi2a(int num, char *bf);

/* Save/restore bookkeeping data, to avoid partially writing a string. */
struct rbufsav {
    size_t rd;
    size_t wr;
    size_t len;
};
static void rbuf_save(struct ringbuf *r, struct rbufsav *sav);
static void rbuf_load(struct ringbuf *r, struct rbufsav *sav);

void rbuf_init(struct ringbuf *r, char *mem, size_t size)
{
    r->mem  = mem;
    r->size = size;
    r->rd   = 0;
    r->wr   = 0;
    r->len  = 0;
}

int rbuf_putc(struct ringbuf *r, char c)
{
    if (r->len == r->size) {
        bwprintf(COM2, "\e[s\e[20;1j buffer full! \e[u"); /* FIXME */
        return -1; /* full */
    }

    r->mem[r->wr++] = c;
    r->wr %= r->size;
    r->len++;
    return 0;
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

int rbuf_write(struct ringbuf *r, int n, const char *s)
{
    int i;
    struct rbufsav sav;

    rbuf_save(r, &sav);
    for (i = 0; i < n; i++) {
        int rc = rbuf_putc(r, *s++);
        if (rc != 0) {
            rbuf_load(r, &sav);
            return rc;
        }
    }

    return 0;
}

int rbuf_print(struct ringbuf *r, const char *s)
{
    char c;
    struct rbufsav sav;

    rbuf_save(r, &sav);
    while ((c = *s++)) {
        int rc = rbuf_putc(r, c);
        if (rc != 0) {
            rbuf_load(r, &sav);
            return rc;
        }
    }

    return 0;
}

int rbuf_printf(struct ringbuf *r, char *fmt, ...)
{
    va_list args;
    int rc;
    va_start(args, fmt);
    rc = rbuf_vprintf(r, fmt, args);
    va_end(args);
    return rc;
}

static int rbuf_nputc_x(struct ringbuf *r, int n, char ch)
{
    while (n-- > 0) {
        int rc = rbuf_putc(r, ch);
        if (rc != 0)
            return rc;
    }
    return 0;
}

int rbuf_nputc(struct ringbuf *r, int n, char ch)
{
    struct rbufsav sav;
    int rc;
    rbuf_save(r, &sav);
    rc = rbuf_nputc_x(r, n, ch);
    if (rc != 0)
        rbuf_load(r, &sav);
    return rc;
}

static int rbuf_align_x(
    struct ringbuf *r,
    int w,
    char pad,
    bool left,
    const char *buf)
{
    int rc;

    const char *p = buf;

    while (*p++ && w > 0)
        w--;

    if (!left && (rc = rbuf_nputc_x(r, w, pad)) != 0)
        return rc;

    rc = rbuf_print(r, buf);
    if (rc != 0)
        return rc;

    if (left && (rc = rbuf_nputc_x(r, w, pad)) != 0)
        return rc;

    return 0;
}

static int rbuf_align(
    struct ringbuf *r,
    int width,
    char pad,
    bool left,
    const char *buf)
{
    struct rbufsav sav;
    int rc;
    rbuf_save(r, &sav);
    rc = rbuf_align_x(r, width, pad, left, buf);
    if (rc != 0)
        rbuf_load(r, &sav);
    return rc;
}

int rbuf_alignl(struct ringbuf *r, int w, char pad, const char *s)
{
    return rbuf_align(r, w, pad, true, s);
}

int rbuf_alignr(struct ringbuf *r, int w, char pad, const char *s)
{
    return rbuf_align(r, w, pad, false, s);
}

/* Much the same as bwformat() */
int rbuf_vprintf(struct ringbuf *r, char *fmt, va_list args)
{
    struct rbufsav sav;
    char ch, pad, buf[16];
    int rc, w;
    bool left;

    rbuf_save(r, &sav);
    while ((ch = *fmt++)) {
        if (ch != '%') {
            rc = rbuf_putc(r, ch);
            if (rc == 0)
                continue;
            else {
                rbuf_load(r, &sav);
                return rc;
            }
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
            rc = rbuf_align_x(r, w, pad, left, buf);
            break;

        case 's':
            rc = rbuf_align_x(r, w, pad, left, va_arg(args, char*));
            break;

        case 'u':
            bwui2a(va_arg(args, unsigned), 10, buf);
            rc = rbuf_align_x(r, w, pad, left, buf);
            break;

        case 'd':
            bwi2a(va_arg(args, int), buf);
            rc = rbuf_align_x(r, w, pad, left, buf);
            break;

        case 'x':
            bwui2a(va_arg(args, unsigned), 16, buf);
            rc = rbuf_align_x(r, w, pad, left, buf);
            break;

        case 'p':
            buf[0] = '0';
            buf[1] = 'x';
            bwui2a(va_arg(args, unsigned), 16, buf + 2);
            rc = rbuf_align_x(r, w, pad, left, buf);
            break;

        case '%':
            buf[0] = '%';
            buf[1] = '\0';
            rc = rbuf_align_x(r, w, pad, left, buf);
            break;

        default:
            rc = -1;
        }

        if (rc != 0) {
            rbuf_load(r, &sav);
            return rc;
        }
    }

    return 0;
}

static void rbuf_save(struct ringbuf *r, struct rbufsav *sav)
{
    sav->rd  = r->rd;
    sav->wr  = r->wr;
    sav->len = r->len;
}

static void rbuf_load(struct ringbuf *r, struct rbufsav *sav)
{
    r->rd  = sav->rd;
    r->wr  = sav->wr;
    r->len = sav->len;
}
