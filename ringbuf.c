#include "util.h"
#include "ringbuf.h"

struct rbufsav {
    int rd, wr, len;
};

/* Save/restore bookkeeping data, to avoid partially writing a string. */
static void rbuf_save(struct ringbuf *r, struct rbufsav *sav);
static void rbuf_load(struct ringbuf *r, struct rbufsav *sav);

void rbuf_init(struct ringbuf *r)
{
    r->rd  = 0;
    r->wr  = 0;
    r->len = 0;
}

int rbuf_putc(struct ringbuf *r, char c)
{
    if (r->len == RINGBUFSIZ)
        return -1; /* full */

    r->buf[r->wr++] = c;
    r->wr %= RINGBUFSIZ;
    r->len++;
    return 0;
}

bool rbuf_peekc(struct ringbuf *r, char *c_out)
{
    if (r->len == 0)
        return false;

    *c_out = r->buf[r->rd];
    return true;
}

bool rbuf_getc(struct ringbuf *r, char *c_out)
{
    if (r->len == 0)
        return false;

    *c_out = r->buf[r->rd++];
    r->rd %= RINGBUFSIZ;
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
