#ifdef TEST_LOG_H
#error "double-included test/log.h"
#endif

#define TEST_LOG_H

XARG_H;

struct testlog {
    char *buf;
    int   buflen;
    int   pos;
};

void tlog_init(struct testlog*, char *buf, int len);
void tlog_check(struct testlog*, const char*);
void tlog_putc(struct testlog*, char);
void tlog_putx(struct testlog*, char);
void tlog_putstr(struct testlog*, const char*);
void tlog_putw(struct testlog*, int n, char c, const char *s);
void tlog_vprintf(struct testlog*, const char*, va_list);
void tlog_printf(struct testlog*, const char*, ...)
    __attribute__((format(printf, 2, 3)));
