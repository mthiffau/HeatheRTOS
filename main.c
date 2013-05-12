/* #include "bwio.h" */
/* #include "ts7200.h" */

#include "util.h"
#include "ts7200.h"
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

/* Escape sequences */
#define TERM_RESET_DEVICE           "\ec"
#define TERM_ERASE_ALL              "\e[2J"
#define TERM_ERASE_DOWN             "\e[J"
#define TERM_ERASE_EOL              "\e[K"
#define TERM_FORCE_CURSOR(row, col) "\e[" row ";" col "f" /* 1-indexed */
#define TERM_SAVE_CURSOR            "\e[s"
#define TERM_RESTORE_CURSOR         "\e[u"

/* Program state. */
struct state {
    struct ringbuf out;         /* Output buffer  */
    struct clock   clock;       /* Clock state    */
    int  cmdlen;                /* Command length */
    char cmd[CMD_MAXLEN + 1];   /* Command string */
    bool quit;
};

int init(struct state *st)
{
    int rc;
    rbuf_init(&st->out);
    st->cmdlen = 0;
    st->quit   = false;
    rc = clock_init(&st->clock, CLOCK_Hz);
    if (rc != 0) {
        bwputstr(COM2, "failed to initialize clock at " STR(CLOCK_Hz) " Hz");
        return rc;
    }
    return 0;
}

void draw_init(struct state *st)
{
    rbuf_print(&st->out,
        TERM_RESET_DEVICE
        TERM_ERASE_ALL
        TERM_FORCE_CURSOR(STR(CLOCK_ROW), STR(CLOCK_COL))
        TIME_MSG
        TERM_FORCE_CURSOR(STR(PROMPT_ROW), STR(PROMPT_COL))
        PROMPT);
}

/* FIXME */
int tokenize(char *cmd, char *ts[CMD_MAXLEN / 2])
{
    while (*cmd == ' ')
        cmd++;

    int n = 0;
    while (*cmd != '\0') {
        ts[n++] = cmd;
        while (*cmd != '\0') {
            if (*cmd == ' ') {
                *cmd++ = '\0';
                break;
            }
        }
        while (*cmd == ' ')
            cmd++;
    }

    return n;
}

void runcmd(struct state *st)
{
    char *cmd = st->cmd;
    while (*cmd == ' ') cmd++;
    if (*cmd == 'q')
        st->quit = true;
}

void tty_recv(struct state *st, char c)
{
    switch (c) {
    case '\r':
    case '\n':
        runcmd(st);
        rbuf_print(&st->out,
            TERM_FORCE_CURSOR(STR(PROMPT_ROW), STR(CMD_COL))
            TERM_ERASE_EOL);
        st->cmdlen = 0;
        st->cmd[0] = '\0';
        break;

    case '\b':
        if (st->cmdlen > 0) {
            st->cmd[--st->cmdlen] = '\0';
            rbuf_print(&st->out, "\b \b");
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
        rbuf_putc(&st->out, c);
        break;
    }
}

void tty_check(struct state *st)
{
    char c;
    if (p_trygetc(P_TTY, &c))
        tty_recv(st, c);
}

void tty_send(struct state *st)
{
    char c;
    if (rbuf_peekc(&st->out, &c) && p_tryputc(P_TTY, c))
        rbuf_getc(&st->out, &c); /* remove the peeked character */
}

void clock_print(struct state *st)
{
    uint32_t ticks, tenths, seconds, minutes;
    ticks   = clock_ticks(&st->clock);
    tenths  = ticks % 10;
    ticks   = ticks / 10;
    seconds = ticks % 60;
    ticks   = ticks / 60;
    minutes = ticks;
    rbuf_printf(&st->out,
        TERM_SAVE_CURSOR
        TERM_FORCE_CURSOR(STR(CLOCK_ROW), STR(TIME_COL))
        "%02u:%02u.%u"
        TERM_RESTORE_CURSOR,
        minutes, seconds, tenths);
}

void clock_check(struct state *st)
{
    if (clock_update(&st->clock))
        clock_print(st);
}

int main(int argc, char* argv[])
{
    struct state st;

    /* ignore arguments */
    (void)argc;
    (void)argv;

    /* disable FIFOs */
    p_enablefifo(P_TTY,   false);
    p_enablefifo(P_TRAIN, false);

    /* initialize */
    init(&st);
    draw_init(&st);

    /* main loop */
    while (!st.quit) {
        tty_check(&st);
        clock_check(&st);
        tty_send(&st);
    }

    return 0;
}
