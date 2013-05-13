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
 *
 *     Sensors: A1 B12 ...
 *
 *     Switches:
 *     1  2  3  4  5 ...
 *     S  S  C  S  C ...
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

#define SENSORS_ROW      5          /* Sensor list */
#define SENSORS_COL      1
#define SENSORS_MSG      "Sensors: "
#define SENSOR_LIST_COL  10

#define SWITCHES_TTL_ROW 7
#define SWITCHES_TTL_COL 1
#define SWITCHES_TTL     "Switches:"
#define SWITCHES_LBL_ROW 8
#define SWITCHES_LBL_COL 1
#define SWITCHES_LBL     "  1  2  3  4  5  6  7  8  9" \
                         " 10 11 12 13 14 15 16 17 18" \
                         "x99x9Ax9Bx9C\n" \
                         "  S  S  S  S  S  S  S  S  S" \
                         "  S  S  S  S  S  S  S  S  S" \
                         "  S  S  S  S"
#define SWITCHES_ROW     9
#define SWITCHES_COL     1

/* Escape sequences */
#define TERM_RESET_DEVICE           "\ec"
#define TERM_ERASE_ALL              "\e[2J"
#define TERM_ERASE_DOWN             "\e[J"
#define TERM_ERASE_EOL              "\e[K"
#define TERM_FORCE_CURSOR(row, col) "\e[" row ";" col "f" /* 1-indexed */
#define TERM_SAVE_CURSOR            "\e[s"
#define TERM_RESTORE_CURSOR         "\e[u"

/* Trains and train commands */
#define NTRAINS                256
#define TRAIN_STOP_TICKS       25 /* 2.5 seconds */
#define SENSOR_HISTORY_MAX     10
#define SENSOR_NMODULES        5

#define TRCMD_SPEED(n)         ((char)n)
#define TRCMD_REVERSE          ((char)15)
#define TRCMD_SWITCH_STRAIGHT  ((char)33)
#define TRCMD_SWITCH_CURVE     ((char)34)
#define TRCMD_SWITCH_OFF       ((char)32)
#define TRCMD_SENSOR_POLL_ALL  ((char)133)

typedef uint8_t trainid;
STATIC_ASSERT(trainid_large_enough, (trainid)(NTRAINS - 1) == NTRAINS - 1);

struct train {
    uint8_t speed;
    bool    rev;          /* reversing? */
    int     rvstart;      /* reverse start time */
    bool    rvhasnext;    /* is next valid? */
    trainid rvnext;       /* next reversing train */
};

/* Program state. */
#define TTYOUT_BUFSIZE   1024
#define TROUT_BUFSIZE    128

/* Program state */
struct state {
    /* Output buffers */
    struct ringbuf ttyout;
    struct ringbuf trout;
    char trout_mem [TROUT_BUFSIZE];
    char ttyout_mem[TTYOUT_BUFSIZE];

    /* Clock state  */
    struct clock clock;

    /* Command state */
    int  cmdlen;
    char cmd[CMD_MAXLEN + 1]; /* NUL-terminated */
    bool quit;

    /* Train state */
    struct train trains[NTRAINS];
    int          trv_head; /* negative is invalid */
    int          trv_last;

    /* Sensor state */
    uint8_t sensors [SENSOR_HISTORY_MAX]; /* mmmmssss - m module, s sensor */
    int     nsensors;                     /* number of recent sensors */
    int     sens_ix;                      /* where to write next value */
    int     sens_poll_mod;                /* module coming next */
    bool    sens_poll_high;               /* low or high byte */
};

int init(struct state *st)
{
    int i, rc, sw;

    /* Console port setup */
    p_enable_fifo(P_TTY, false);

    /* Train port setup */
    p_enable_fifo(P_TRAIN, false);
    p_set_baudrate(P_TRAIN, 2400);
    p_enable_parity(P_TRAIN, false);
    p_enable_2stop(P_TRAIN, true);

    /* Output buffer setup */
    rbuf_init(&st->ttyout, st->ttyout_mem, TTYOUT_BUFSIZE);
    rbuf_init(&st->trout,  st->trout_mem,  TROUT_BUFSIZE);

    /* Command-line setup */
    st->cmdlen = 0;
    st->quit   = false;

    /* Train data setup */
    for (i = 0; i < NTRAINS; i++) {
        struct train *train = &st->trains[i];
        train->speed = 0;
        train->rev   = false;
    }
    st->trv_head = st->trv_last = -1;

    /* Sensor history setup */
    st->sens_ix        = 0;
    st->nsensors       = 0;
    st->sens_poll_mod  = 0;
    st->sens_poll_high = false;

    /* Initial output: reset terminal, clear screen, print static text. */
    rbuf_print(&st->ttyout,
        TERM_RESET_DEVICE
        TERM_ERASE_ALL
        TERM_FORCE_CURSOR(STR(CLOCK_ROW), STR(CLOCK_COL))
        TIME_MSG
        TERM_FORCE_CURSOR(STR(SENSORS_ROW), STR(SENSORS_COL))
        SENSORS_MSG
        TERM_FORCE_CURSOR(STR(SWITCHES_TTL_ROW), STR(SWITCHES_TTL_COL))
        SWITCHES_TTL
        TERM_FORCE_CURSOR(STR(SWITCHES_LBL_ROW), STR(SWITCHES_LBL_COL))
        SWITCHES_LBL
        TERM_FORCE_CURSOR(STR(PROMPT_ROW), STR(PROMPT_COL))
        PROMPT);

    for (sw = 1; sw <= 18; sw++) {
        rbuf_putc(&st->trout, TRCMD_SWITCH_STRAIGHT);
        rbuf_putc(&st->trout, sw);
    }
    for (sw = 0x99; sw <= 0x9c; sw++) {
        rbuf_putc(&st->trout, TRCMD_SWITCH_STRAIGHT);
        rbuf_putc(&st->trout, sw);
    }
    rbuf_putc(&st->trout, TRCMD_SWITCH_OFF);

    /* Clock setup */
    rc = clock_init(&st->clock, CLOCK_Hz);
    if (rc != 0) {
        bwputstr(COM2, "failed to initialize clock at " STR(CLOCK_Hz) " Hz");
        return rc;
    }

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
    unsigned n = 0, base = 10;
    char c;
    if (s[0] == '0' && s[1] == 'x') {
        base = 16;
        s += 2;
    }
    while ((c = *s++) != '\0') {
        if (c >= '0' && c <= '9') {
            n = n * base + c - '0';
        } else if (base == 16 && c >= 'a' && c <= 'f') {
            n = n * base + c - 'a' + 10;
        } else if (base == 16 && c >= 'A' && c <= 'F') {
            n = n * base + c - 'A' + 10;
        } else {
            return -1;
        }

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

void cmd_msg_clear(struct state *st)
{
    rbuf_print(&st->ttyout,
        TERM_SAVE_CURSOR
        TERM_FORCE_CURSOR(STR(CMD_MSG_ROW), STR(CMD_MSG_COL))
        TERM_ERASE_EOL
        TERM_RESTORE_CURSOR);
}

void runcmd_q(struct state *st, int argc, char *argv[])
{
    (void)argv; /* ignore */
    if (argc != 0) {
        cmd_msg_printf(st, "usage: q");
        return;
    }

    cmd_msg_clear(st);
    st->quit = true;
}

void runcmd_tr(struct state *st, int argc, char *argv[])
{
    /* Parse arguments */
    struct train *train;
    trainid which;
    uint8_t speed;
    if (argc != 2) {
        cmd_msg_printf(st, "usage: tr TRAIN SPEED");
        return;
    }

    if (atou8(argv[0], &which) != 0) {
        cmd_msg_printf(st, "bad train '%s'", argv[0]);
        return;
    }

    if (atou8(argv[1], &speed) != 0 || speed > 14) {
        cmd_msg_printf(st, "bad speed '%s'", argv[1]);
        return;
    }

    /* Send command */
    cmd_msg_clear(st);
    train = &st->trains[which];
    train->speed = speed;
    if (!train->rev) { /* don't send speed while reversing  */
        rbuf_putc(&st->trout, (char)speed);
        rbuf_putc(&st->trout, (char)which);
    }
}

void runcmd_rv(struct state *st, int argc, char *argv[])
{
    /* Parse arguments */
    trainid which;
    struct train *train;
    if (argc != 1) {
        cmd_msg_printf(st, "usage: rv TRAIN");
        return;
    }

    if (atou8(argv[0], &which) != 0) {
        cmd_msg_printf(st, "bad train '%s'", argv[1]);
        return;
    }

    /* Reject trains that are already reversing */
    train = &st->trains[which];
    if (train->rev) {
        cmd_msg_printf(st, "train %u already reversing", which);
        return;
    }

    /* Start reversing the train. */
    cmd_msg_clear(st);
    rbuf_putc(&st->trout, TRCMD_SPEED(0));
    rbuf_putc(&st->trout, (char)which);

    /* Enqueue this train. */
    train->rev       = true;
    train->rvstart   = st->clock.ticks;
    train->rvhasnext = false;
    if (st->trv_head < 0) {
        st->trv_head = st->trv_last = which;
    } else {
        struct train *last = &st->trains[st->trv_last];
        last->rvhasnext = true;
        last->rvnext    = which;
        st->trv_last    = which;
    }
}

void rv_continue(struct state *st)
{
    struct train *train;
    trainid id;
    int head = st->trv_head;
    if (head < 0)
        return;

    id = head;
    train = &st->trains[id];
    if (!train->rev) {
        /* sanity check! */
        bwprintf(COM2,
            TERM_SAVE_CURSOR
            TERM_FORCE_CURSOR("4", "1")
            "assertion failed! non-reversing train in reversing train queue"
            TERM_RESTORE_CURSOR);
        st->quit = true;
        return;
    }

    if (st->clock.ticks - train->rvstart < TRAIN_STOP_TICKS)
        return; /* hasn't been long enough */

    /* done waiting for train to stop. Reverse!
     * and clear its reversing state */
    train->rev   = false;
    st->trv_head = train->rvhasnext ? train->rvnext : -1;
    rbuf_putc(&st->trout, TRCMD_REVERSE);
    rbuf_putc(&st->trout, (char)id);
    rbuf_putc(&st->trout, TRCMD_SPEED(train->speed));
    rbuf_putc(&st->trout, (char)id);
}

int print_switch(struct state *st, int sw, char desc)
{
    if (sw < 1 || (sw > 18 && sw < 0x99) || sw > 0x9c)
        return -1; /* unknown switch */

    if (sw > 18)
        sw += 19 - 0x99;

    rbuf_printf(&st->ttyout,
        TERM_SAVE_CURSOR
        TERM_FORCE_CURSOR(STR(SWITCHES_ROW), "%d")
        "%c"
        TERM_RESTORE_CURSOR,
        3 * sw, /* column */
        desc);

    return 0;
}

void runcmd_sw(struct state *st, int argc, char *argv[])
{
    /* Parse arguments */
    uint8_t sw;
    uint8_t dir, dir_err;
    char    desc;
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
                desc = 'S';
                dir  = TRCMD_SWITCH_STRAIGHT;
                break;
            case 'c':
            case 'C':
                desc = 'C';
                dir  = TRCMD_SWITCH_CURVE;
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
    if (print_switch(st, sw, desc) != 0) {
        cmd_msg_printf(st, "unrecognized switch '%s'", argv[0]);
        return;
    }

    cmd_msg_clear(st);
    rbuf_putc(&st->trout, dir);
    rbuf_putc(&st->trout, (char)sw);
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
    else if (!strcmp(cmd, "rv"))
        runcmd_rv(st, nts - 1, ts + 1);
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

void sensor_recv(struct state *st, uint8_t c)
{
    int  i, j, bit;
    int  mod  = (st->sens_poll_mod << 4) | (st->sens_poll_high ? 0x8 : 0);
    bool trip = false;

    if (!st->sens_poll_high)
        st->sens_poll_high = true;
    else {
        st->sens_poll_mod = (st->sens_poll_mod + 1) % SENSOR_NMODULES;
        st->sens_poll_high = false;
    }

    for (i = 0, bit = 1; i < 8; i++, bit <<= 1) {
        if (!(c & bit))
            continue;

        trip = true;
        st->sensors[st->sens_ix++] = mod | (7 - i);
        st->sens_ix &= 0x7;
        if (st->nsensors < SENSOR_HISTORY_MAX)
            st->nsensors++;
    }

    if (!trip)
        return;

    rbuf_print(&st->ttyout,
        TERM_SAVE_CURSOR
        TERM_FORCE_CURSOR(STR(SENSORS_ROW), STR(SENSOR_LIST_COL))
        TERM_ERASE_EOL);
    j = (st->sens_ix - 1) & 0x7;
    for (i = 0; i < st->nsensors; i++, j = (j - 1) & 0x7) {
        uint8_t sens = st->sensors[j];
        uint8_t mod  = sens >> 4;
        sens         = sens & 0xf;
        rbuf_printf(&st->ttyout, "%c%u ", 'A' + mod, sens + 1);
    }
    rbuf_print(&st->ttyout, TERM_RESTORE_CURSOR);
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
    bool train_drain = true; /* ignoring initial bytes from train */
    while (!st.quit) {
        char c;
        if (p_trygetc(P_TTY, &c))
            tty_recv(&st, c);

        if (p_trygetc(P_TRAIN, &c)) {
            if (!train_drain)
                sensor_recv(&st, (uint8_t)c);
        }

        if (clock_update(&st.clock)) {
            clock_print(&st);
            rv_continue(&st);
            rbuf_putc(&st.trout, TRCMD_SENSOR_POLL_ALL);
            train_drain = false;
        }

        /* send the first queued byte to TTY if possible */
        if (rbuf_peekc(&st.ttyout, &c) && p_tryputc(P_TTY, c))
            rbuf_getc(&st.ttyout, &c);

        /* and to the train */
        if (rbuf_peekc(&st.trout, &c)) {
            if (p_cts(P_TRAIN) && p_tryputc(P_TRAIN, c))
                rbuf_getc(&st.trout, &c);
        }
    }

    bwputstr(COM2, TERM_RESET_DEVICE TERM_ERASE_ALL); /* FIXME */

    return 0;
}
