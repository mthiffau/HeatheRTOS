#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "xarg.h"
#include "xstring.h"
#include "ts7200.h"
#include "static_assert.h"
#include "serial.h"
#include "clock.h"
#include "ringbuf.h"
#include "bwio.h"  /* bwputstr if clock initialization failed */

#define STR(x)  STR1(x)
#define STR1(x) #x

#define CLOCK_Hz         10

/* Screen layout:
 *
 *     Time: 01:23.4
 *     > some command up to CMD_MAXLEN chars
 *     status message
 */
#define CLOCK_ROW        1          /* Clock */
#define CLOCK_COL        1
#define TIME_MSG         "Time: "
#define TIME_COL         7

#define PROMPT           "> "       /* Command prompt */
#define PROMPT_WIDTH     2
#define PROMPT_ROW       2
#define PROMPT_COL       1
#define CMD_COL          3
#define CMD_MAXLEN       72
#define CMD_MAXTOKS      3
#define CMD_MSG_ROW      3
#define CMD_MSG_COL      1

/* Escape sequences */
#define TERM_RESET_DEVICE           "\ec"
#define TERM_ERASE_ALL              "\e[2J"
#define TERM_ERASE_DOWN             "\e[J"
#define TERM_ERASE_EOL              "\e[K"
#define TERM_FORCE_CURSOR(row, col) "\e[" row ";" col "f" /* 1-indexed */
#define TERM_SAVE_CURSOR            "\e[s"
#define TERM_RESTORE_CURSOR         "\e[u"

/* Train commands */
#define TRCMD_SWITCH_STRAIGHT  34
#define TRCMD_SWITCH_CURVE     33
#define TRCMD_SWITCH_OFF       32

/* Program state. */
#define TTYOUT_BUFSIZE   1024
#define TROUT_BUFSIZE    128

struct state {
    struct ringbuf ttyout;      /* Output buffers */
    struct ringbuf trout;       /* Output buffers */
    struct clock   clock;       /* Clock state    */
    int  cmdlen;                /* Command length */
    char cmd[CMD_MAXLEN + 1];   /* Command string */
    bool quit;
    char trout_mem [TROUT_BUFSIZE];  /* Buffer memory */
    char ttyout_mem[TTYOUT_BUFSIZE]; /* Buffer memory */
};

int init(struct state *st)
{
    int rc;

    /* Console setup */
    p_enable_fifo(P_TTY, false);

    /* Train setup */
    p_enable_fifo(P_TRAIN, false);
    p_set_baudrate(P_TRAIN, 2400);
    p_enable_parity(P_TRAIN, false);
    p_enable_2stop(P_TRAIN, true);

    /* Set up data */
    rbuf_init(&st->ttyout, st->ttyout_mem, TTYOUT_BUFSIZE);
    rbuf_init(&st->trout,  st->trout_mem,  TROUT_BUFSIZE);
    st->cmdlen = 0;
    st->quit   = false;
    rc = clock_init(&st->clock, CLOCK_Hz);
    if (rc != 0) {
        bwputstr(COM2, "failed to initialize clock at " STR(CLOCK_Hz) " Hz");
        return rc;
    }

    /* Print initial output - reset terminal, clear screen, etc */
    rbuf_print(&st->ttyout,
        TERM_RESET_DEVICE
        TERM_ERASE_ALL
        TERM_FORCE_CURSOR(STR(CLOCK_ROW), STR(CLOCK_COL))
        TIME_MSG
        TERM_FORCE_CURSOR(STR(PROMPT_ROW), STR(PROMPT_COL))
        PROMPT);

    return 0;
}

/* FIXME */
int tokenize(char *cmd, char **ts, int max_ts)
{
    int n = 0;
    while (*cmd == ' ')
        cmd++;

    while (*cmd != '\0') {
        char c;
        if (n >= max_ts)
            return -1; /* too many tokens */

        ts[n++] = cmd++;
        for (;;) {
            c = *cmd;
            if (c == '\0' || c == ' ')
                break;
            cmd++;
        }

        if (*cmd == ' ')
            *cmd++ = '\0';

        while (*cmd == ' ')
            cmd++;
    }

    return n;
}

int atou8(const char *s, uint8_t *ret)
{
    unsigned n = 0;
    char c;
    while ((c = *s++) != '\0') {
        if (c < '0' || c > '9')
            return -1;

        n = n * 10 + c - '0';
        if (n > 255)
            return -1;
    }

    *ret = (uint8_t)n;
    return 0;
}

void cmd_msg_printf(struct state *st, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

void cmd_msg_printf(struct state *st, const char *fmt, ...)
{
    va_list args;
    rbuf_print(&st->ttyout,
        TERM_SAVE_CURSOR
        TERM_FORCE_CURSOR(STR(CMD_MSG_ROW), STR(CMD_MSG_COL))
        TERM_ERASE_EOL);

    va_start(args, fmt);
    rbuf_vprintf(&st->ttyout, fmt, args);
    va_end(args);

    rbuf_print(&st->ttyout, TERM_RESTORE_CURSOR);
}

void runcmd_q(struct state *st, int argc, char *argv[])
{
    (void)argv; /* ignore */
    if (argc == 0)
        st->quit = true;
    else
        cmd_msg_printf(st, "usage: q");
}

void runcmd_tr(struct state *st, int argc, char *argv[])
{
    uint8_t train, speed;
    /* Parse arguments */
    if (argc != 2) {
        cmd_msg_printf(st, "usage: tr TRAIN SPEED");
        return;
    }

    if (atou8(argv[0], &train) != 0) {
        cmd_msg_printf(st, "bad train '%s'", argv[0]);
        return;
    }

    if (atou8(argv[1], &speed) != 0 || speed > 15) {
        cmd_msg_printf(st, "bad speed '%s'", argv[1]);
        return;
    }

    /* Send command */
    rbuf_putc(&st->trout, (char)speed);
    rbuf_putc(&st->trout, (char)train);
}

void runcmd_sw(struct state *st, int argc, char *argv[])
{
    uint8_t sw;
    uint8_t dir, dir_err;
    /* Parse arguments */
    if (argc != 2) {
        cmd_msg_printf(st, "usage: sw SWITCH [SC]");
        return;
    }

    if (atou8(argv[0], &sw) != 0) {
        cmd_msg_printf(st, "bad switch '%s'", argv[0]);
        return;
    }

    dir_err = false;
    if (argv[1][1] != '\0') {
        dir_err = true;
    } else {
        switch (argv[1][0]) {
            case 's':
            case 'S':
                dir = TRCMD_SWITCH_STRAIGHT;
                break;
            case 'c':
            case 'C':
                dir = TRCMD_SWITCH_CURVE;
                break;
            default:
                dir_err = true;
        }
    }

    if (dir_err) {
        cmd_msg_printf(st, "bad direction '%s'", argv[1]);
        return;
    }

    /* Send command */

    rbuf_putc(&st->trout, dir);
    rbuf_putc(&st->trout, sw);
    rbuf_putc(&st->trout, TRCMD_SWITCH_OFF);
}

void runcmd(struct state *st)
{
    int   nts;
    char *cmd, *ts[CMD_MAXTOKS];

    nts = tokenize(st->cmd, ts, CMD_MAXTOKS);
    if (nts <= 0) {
        /* ignore */
        return;
    }

    cmd = ts[0];
    if (!strcmp(cmd, "q"))
        runcmd_q(st, nts - 1, ts + 1);
    else if (!strcmp(cmd, "tr"))
        runcmd_tr(st, nts - 1, ts + 1);
    else if (!strcmp(cmd, "sw"))
        runcmd_sw(st, nts - 1, ts + 1);
    else
        cmd_msg_printf(st, "unrecognized command");
}

void tty_recv(struct state *st, char c)
{
    switch (c) {
    case '\r':
    case '\n':
        runcmd(st);
        rbuf_print(&st->ttyout,
            TERM_FORCE_CURSOR(STR(PROMPT_ROW), STR(CMD_COL))
            TERM_ERASE_EOL);
        st->cmdlen = 0;
        st->cmd[0] = '\0';
        break;

    case '\b':
        if (st->cmdlen > 0) {
            st->cmd[--st->cmdlen] = '\0';
            rbuf_print(&st->ttyout, "\b \b");
        }
        break;

    default:
        if (st->cmdlen == CMD_MAXLEN)
            break;
        else if (c < ' ' || c > '~') {
            if (c == '\t')
                c = ' ';
            else
                break;
        }
        st->cmd[st->cmdlen] = c;
        st->cmd[++st->cmdlen] = '\0';
        rbuf_putc(&st->ttyout, c);
        break;
    }
}

void clock_print(struct state *st)
{
    uint32_t ticks, tenths, seconds, minutes;
    ticks   = st->clock.ticks;
    tenths  = ticks % 10;
    ticks   = ticks / 10;
    seconds = ticks % 60;
    ticks   = ticks / 60;
    minutes = ticks;
    rbuf_printf(&st->ttyout,
        TERM_SAVE_CURSOR
        TERM_FORCE_CURSOR(STR(CLOCK_ROW), STR(TIME_COL))
        "%02u:%02u.%u"
        TERM_RESTORE_CURSOR,
        minutes, seconds, tenths);
}

int main(int argc, char* argv[])
{
    struct state st;

    /* ignore arguments */
    (void)argc;
    (void)argv;

    /* initialize */
    init(&st);

    /* main loop */
    while (!st.quit) {
        char c;
        if (p_trygetc(P_TTY, &c))
            tty_recv(&st, c);

        if (clock_update(&st.clock))
            clock_print(&st);

        /* send the first queued byte to TTY if possible */
        if (rbuf_peekc(&st.ttyout, &c) && p_tryputc(P_TTY, c))
            rbuf_getc(&st.ttyout, &c);

        /* and to the train */
        if (rbuf_peekc(&st.trout, &c)) {
            if (p_cts(P_TRAIN) && p_tryputc(P_TRAIN, c))
                rbuf_getc(&st.trout, &c);
        }
    }

    return 0;
}
