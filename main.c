/* #include "bwio.h" */
/* #include "ts7200.h" */

#include "util.h"
#include "serial.h"
#include "clock.h"
#include "ringbuf.h"

#include "bwio.h"  /* bwprintf */

#define CLOCK_Hz         10

#define CLOCK_ROW        1
#define CMDLINE_ROW      2
#define CMDPROMPT        "> "
#define CMDPROMPT_WIDTH  2

#define MAX_CMD_LEN      72

#define ERASE_ALL    "\e[2J"
#define ERASE_EOL    "\e[K"
#define RESET_DEVICE "\ec"
#define TERM_INIT    RESET_DEVICE ERASE_ALL

#define MOVE_CURSOR  "\e[%d;%df"
#define CURSOR_LEFT1 "\e[D"

struct cmdline {
    int  len;
    char buf[MAX_CMD_LEN + 1];
};

void cmdl_init(struct cmdline *cmdl)
{
    cmdl->len = 0;
}

void cmdl_print_init(struct ringbuf *out)
{
    rbuf_printf(out, MOVE_CURSOR CMDPROMPT, CMDLINE_ROW, 1);
}

void cmdl_set_cursor(struct cmdline *cmdl, struct ringbuf *out)
{
    int col = 1 + CMDPROMPT_WIDTH + cmdl->len;
    rbuf_printf(out, MOVE_CURSOR, CMDLINE_ROW, col);
}

void cmdl_backspace(struct cmdline *cmdl, struct ringbuf *out)
{
    if (cmdl->len == 0) {
        rbuf_putc(out, '\a');
    } else {
        cmdl->buf[cmdl->len--] = '\0';
        rbuf_print(out, "\b \b");
    }
}

/* append printable character to command-line */
void cmdl_append(struct cmdline *cmdl, char ch, struct ringbuf *out)
{
    if (cmdl->len == MAX_CMD_LEN) {
        rbuf_putc(out, '\a');
    } else {
        cmdl->buf[cmdl->len++] = ch;
        cmdl->buf[cmdl->len] = '\0';
        rbuf_putc(out, ch);
    }
}

void cmdl_accept(struct cmdline *cmdl, struct ringbuf *out)
{
    cmdl->len = 0;
    cmdl_set_cursor(cmdl, out);
    rbuf_printf(out, ERASE_EOL MOVE_CURSOR ERASE_EOL, CMDLINE_ROW + 2, 3);
    rbuf_print(out, cmdl->buf);
    cmdl_set_cursor(cmdl, out);
}

static inline bool isprintable(char c)
{
    return c >= ' ' && c <= '~';
}

/* Cursor should be as set by cmdl_set_cursor */
void cmdl_process(struct cmdline *cmdl, char ch, struct ringbuf *out)
{
    if (ch == '\b') {
        cmdl_backspace(cmdl, out);
    } else if (ch == '\r' || ch == '\n') {
        cmdl_accept(cmdl, out);
    } else if (isprintable(ch)) {
        cmdl_append(cmdl, ch, out);
    } else {
        rbuf_putc(out, '\a');
    }
}

void print_clock(struct ringbuf *out, struct clock *clock)
{
    uint32_t ticks, tenths, seconds, minutes;
    ticks   = clock_ticks(clock);
    tenths  = ticks % 10;
    ticks   = ticks / 10;
    seconds = ticks % 60;
    ticks   = ticks / 60;
    minutes = ticks;
    rbuf_printf(out,
        MOVE_CURSOR "Time: %02u:%02u.%u",
        CLOCK_ROW, 1, minutes, seconds, tenths);
}

int main(int argc, char* argv[])
{
    struct ringbuf out;
    struct clock   clock;
    struct cmdline cmdl;

    /* ignore arguments */
    (void)argc;
    (void)argv;

    /* disable FIFO */
    p_enablefifo(P_TTY, false);

    /* initialize */
    rbuf_init(&out);
    cmdl_init(&cmdl);
    if (clock_init(&clock, CLOCK_Hz) != 0) {
        bwprintf(COM2, "failed to initialize clock at %d Hz\n", CLOCK_Hz);
        return 1;
    }

    /* clear screen before sending any other output */
    rbuf_print(&out, TERM_INIT);

    /* command-line controls the cursor */
    cmdl_print_init(&out);
    cmdl_set_cursor(&cmdl, &out);

    /* main loop */
    for (;;) {
        char c;
        if (p_trygetc(P_TTY, &c))
            cmdl_process(&cmdl, c, &out);

        if (clock_update(&clock)) {
            print_clock(&out, &clock);
            cmdl_set_cursor(&cmdl, &out);
        }

        if (rbuf_peekc(&out, &c)) {
            if (p_tryputc(P_TTY, c))
                rbuf_getc(&out, &c);
        }
    }

    return 0;
}
