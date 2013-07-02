#ifdef DBGLOG_SRV_H
#error "double-included dbglog_srv.h"
#endif

#define DBGLOG_SRV_H

U_TID_H;
XDEF_H;

/* Debug log server entry point */
void dbglogsrv_main(void);

struct dbglogctx {
    tid_t dbglogsrv_tid;
};

void dbglogctx_init(struct dbglogctx *ctx);
void dbglog(struct dbglogctx *ctx, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

int dbglog_read(struct dbglogctx *ctx, char *buf, size_t bufsize);
