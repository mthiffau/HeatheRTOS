#include "xarg.h"
#include "test/log.h"

#include "xbool.h"
#include "xstring.h"
#include "xassert.h"

/* FIXME move these functions out of bwio.c, or use ringbuf stuff? */
extern char c2x(char ch);
extern int bwa2d(char ch);
extern char bwa2i(char ch, const char **src, int base, int *nump);
extern void bwui2a(unsigned int num, unsigned int base, char *bf);
extern void bwi2a(int num, char *bf);

void
tlog_init(struct testlog *log, char *buf, int len)
{
    assert(len > 0);
    log->buf    = buf;
    log->buflen = len;
    log->pos    = 0;
}

void
tlog_check(struct testlog *log, const char *expected)
{
    tlog_putc(log, '\0');
    assert(strcmp(log->buf, expected) == 0);
}

void
tlog_putc(struct testlog *log, char c)
{
    log->buf[log->pos++] = c;
    assert(log->pos < log->buflen);
}

void
tlog_putx(struct testlog *log, char c)
{
    tlog_putc(log, c2x((unsigned)c / 16));
    tlog_putc(log, c2x((unsigned)c % 16));
}

void
tlog_putstr(struct testlog *log, const char *str)
{
    char c;
    while ((c = *str++))
        tlog_putc(log, c);
}

void
tlog_putw(struct testlog *log, int n, char fc, const char *bf)
{
    const char *p = bf;
    while (*p++ && n > 0)
        n--;
    while (n-- > 0)
        tlog_putc(log, fc);
    tlog_putstr(log, bf);
}

void
tlog_vprintf(struct testlog *log, const char *fmt, va_list va)
{
    char bf[12];
    char ch, lz;
    int w;

    while ((ch = *(fmt++))) {
        if ( ch != '%' )
            tlog_putc(log, ch);
        else {
            lz = 0; w = 0;
            ch = *(fmt++);
            switch ( ch ) {
            case '0':
                lz = 1; ch = *(fmt++);
                break;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                ch = bwa2i( ch, &fmt, 10, &w );
                break;
            }
            switch( ch ) {
            case 0: return;
            case 'c':
                tlog_putc(log, va_arg(va, char));
                break;
            case 's':
                tlog_putw(log, w, 0, va_arg(va, char*));
                break;
            case 'u':
                bwui2a( va_arg( va, unsigned int ), 10, bf );
                tlog_putw(log, w, lz, bf);
                break;
            case 'd':
                bwi2a( va_arg( va, int ), bf );
                tlog_putw(log, w, lz, bf);
                break;
            case 'x':
            case 'p':
                bwui2a( va_arg( va, unsigned int ), 16, bf );
                tlog_putw(log, w, lz, bf);
                break;
            case '%':
                tlog_putc(log, ch);
                break;
            }
        }
    }
}

void
tlog_printf(struct testlog *log, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    tlog_vprintf(log, fmt, args);
    va_end(args);
}
