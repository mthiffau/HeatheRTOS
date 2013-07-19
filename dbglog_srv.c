#include "u_tid.h"
#include "xdef.h"
#include "dbglog_srv.h"

#include "xbool.h"
#include "xint.h"
#include "xarg.h"
#include "xassert.h"
#include "xmemcpy.h"
#include "ringbuf.h"

#include "u_syscall.h"
#include "ns.h"

#define DBGLOG_CHUNK_SIZE   128
#define DBGLOG_BUF_SIZE     (1 << 17)  /* 128 kB */

enum {
    DBGLOGMSG_LOG,
    DBGLOGMSG_READ,
};

struct dbglog {
    struct ringbuf log;
    char           log_mem[DBGLOG_BUF_SIZE];
    tid_t          reader;
    size_t         reader_maxlen;
};

struct dbglogmsg {
    int    type;
    size_t len;
    char   buf[DBGLOG_CHUNK_SIZE];
};

static void dbglogsrv_trysend(struct dbglog *log);

void
dbglogsrv_main(void)
{
    struct dbglog log;
    struct dbglogmsg msg;
    tid_t client;
    int rc, msglen;

    log.reader = -1;
    rbuf_init(&log.log, log.log_mem, sizeof (log.log_mem));

    rc = RegisterAs("dbglog");
    assertv(rc, rc == 0);

    for (;;) {
        msglen = Receive(&client, &msg, sizeof (msg));
        assertv(msglen, msglen == sizeof (msg));
        switch (msg.type) {
        case DBGLOGMSG_LOG:
            rc = Reply(client, NULL, 0);
            assertv(rc, rc == 0);
            assert(log.log.len + msg.len <= log.log.size);
            rbuf_write(&log.log, msg.buf, msg.len);
            rbuf_putc(&log.log, '\n');
            break;
        case DBGLOGMSG_READ:
            log.reader = client;
            log.reader_maxlen = msg.len;
            if (log.reader_maxlen > DBGLOG_CHUNK_SIZE)
                log.reader_maxlen = DBGLOG_CHUNK_SIZE;
            break;
        default:
            panic("invalid debug log message type %d", msg.type);
        }
        dbglogsrv_trysend(&log);
    }
}

static void
dbglogsrv_trysend(struct dbglog *log)
{
    struct dbglogmsg msg;
    char c;
    unsigned i;
    int rc;
    if (log->reader < 0 || !rbuf_peekc(&log->log, &c))
        return;

    i = 0;
    while (i < log->reader_maxlen && rbuf_getc(&log->log, &c))
        msg.buf[i++] = c;

    msg.type = DBGLOGMSG_READ;
    msg.len  = i;

    rc = Reply(log->reader, &msg, sizeof (msg));
    assertv(rc, rc == 0);
    log->reader = -1;
}

void
dbglogctx_init(struct dbglogctx *ctx)
{
    ctx->dbglogsrv_tid = WhoIs("dbglog");
}

void
dbglog(struct dbglogctx *ctx, const char *fmt, ...)
{
    struct dbglogmsg msg;
    struct ringbuf rbuf;
    va_list args;
    int rplylen;

    rbuf_init(&rbuf, msg.buf, sizeof (msg.buf));
    va_start(args, fmt);
    rbuf_vprintf(&rbuf, fmt, args);
    va_end(args);

    msg.type = DBGLOGMSG_LOG;
    msg.len  = rbuf.len;
    rplylen  = Send(ctx->dbglogsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

int
dbglog_read(struct dbglogctx *ctx, char *buf, size_t bufsize)
{
    struct dbglogmsg msg, reply;
    int rc;
    msg.type = DBGLOGMSG_READ;
    msg.len  = bufsize < DBGLOG_CHUNK_SIZE ? bufsize : DBGLOG_CHUNK_SIZE;
    rc = Send(ctx->dbglogsrv_tid, &msg, sizeof (msg), &reply, sizeof (reply));
    assertv(rc, rc == sizeof (reply));
    assert(reply.type == DBGLOGMSG_READ);
    assert(reply.len <= bufsize);
    memcpy(buf, reply.buf, reply.len);
    return reply.len;
}
