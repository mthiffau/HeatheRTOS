#include "config.h"
#include "xint.h"
#include "u_tid.h"
#include "ui_srv.h"

#include "xbool.h"
#include "xassert.h"

#include "xdef.h"
#include "xmemcpy.h"
#include "xstring.h"
#include "array_size.h"

#include "u_syscall.h"
#include "ns.h"
#include "clock_srv.h"
#include "serial_srv.h"

#include "xarg.h"
#include "bwio.h"

#define STR(x)          STR1(x)
#define STR1(x)         #x

#define CMD_MAXLEN      63
#define CMD_MAXTOKS     3
#define MAX_SENSORS     10

/* Escape sequences */
#define TERM_RESET_DEVICE           "\ec"
#define TERM_ERASE_ALL              "\e[2J"
#define TERM_ERASE_DOWN             "\e[J"
#define TERM_ERASE_EOL              "\e[K"
#define TERM_FORCE_CURSOR_START     "\e["
#define TERM_FORCE_CURSOR_MID       ";"
#define TERM_FORCE_CURSOR_END       "f"
#define TERM_FORCE_CURSOR(row, col) \
    TERM_FORCE_CURSOR_START row TERM_FORCE_CURSOR_MID col TERM_FORCE_CURSOR_END
#define TERM_SAVE_CURSOR            "\e[s"
#define TERM_RESTORE_CURSOR         "\e[u"

/* Screen positions */
#define TIME_ROW                    2
#define TIME_COL                    7
#define TIME_MSG_COL                1
#define TIME_MSG                    "Time: "
#define CMD_ROW                     4
#define CMD_COL                     3
#define CMD_PROMPT_COL              1
#define CMD_PROMPT                  "> "
#define STATUS_ROW                  6
#define STATUS_COL                  1

enum {
    UIMSG_KEYBOARD,
    UIMSG_SET_TIME,
    UIMSG_SENSORS
};

struct uimsg {
    int type;
    union {
        int     time_tenths;
        char    keypress;
        uint8_t sensors[SENSOR_BYTES];
    };
};

struct uisrv {
    struct serialctx tty;
    struct clkctx    clock;

    char    cmd[CMD_MAXLEN + 1];
    int     cmdlen;

    uint8_t sensors[MAX_SENSORS];
    int     nsensors, lastsensor;
};

static int tokenize(char *cmd, char **ts, int max_ts);

/* FIXME */
static void uisrv_init(struct uisrv *uisrv);
static void uisrv_kbd(struct uisrv *uisrv, char keypress);
static void uisrv_runcmd(struct uisrv *uisrv);
static void uisrv_cmd_q(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_set_time(struct uisrv *uisrv, int tenths);
static void uisrv_sensors(struct uisrv *uisrv, uint8_t sensors[SENSOR_BYTES]);
static void kbd_listen(void);
static void time_listen(void);

void
uisrv_main(void)
{
    struct uisrv uisrv;
    struct uimsg msg;
    tid_t client;
    int rc, msglen;

    uisrv_init(&uisrv);
    rc = RegisterAs("ui");
    assertv(rc, rc == 0);

    for (;;) {
        msglen = Receive(&client, &msg, sizeof (msg));
        assertv(msglen, msglen == sizeof (msg));
        rc = Reply(client, NULL, 0);
        assertv(rc, rc == 0);

        switch (msg.type) {
        case UIMSG_KEYBOARD:
            uisrv_kbd(&uisrv, msg.keypress);
            break;
        case UIMSG_SET_TIME:
            uisrv_set_time(&uisrv, msg.time_tenths);
            break;
        case UIMSG_SENSORS:
            uisrv_sensors(&uisrv, msg.sensors);
            break;
        default:
            panic("Invalid UI server message type %d", msg.type);
        }
    }
}

static void
uisrv_init(struct uisrv *uisrv)
{
    tid_t kbd_tid;
    tid_t time_tid;

    uisrv->cmdlen     = 0;
    uisrv->cmd[0]     = '\0';
    uisrv->nsensors   = 0;
    uisrv->lastsensor = MAX_SENSORS - 1;

    clkctx_init(&uisrv->clock);
    serialctx_init(&uisrv->tty, COM2);

    kbd_tid = Create(1, &kbd_listen);
    assertv(kbd_tid, kbd_tid >= 0);

    time_tid = Create(1, &time_listen);
    assertv(time_tid, time_tid >= 0);

    Print(&uisrv->tty,
        TERM_RESET_DEVICE
        TERM_ERASE_ALL
        TERM_FORCE_CURSOR(STR(TIME_ROW), STR(TIME_MSG_COL))
        TIME_MSG
        TERM_FORCE_CURSOR(STR(CMD_ROW), STR(CMD_PROMPT_COL))
        CMD_PROMPT
        TERM_FORCE_CURSOR(STR(CMD_ROW), STR(CMD_COL)));
}

static void
uisrv_kbd(struct uisrv *uisrv, char keypress)
{
    switch (keypress) {
    case '\r':
    case '\n':
        uisrv_runcmd(uisrv);
        break;

    case '\b':
        if (uisrv->cmdlen > 0) {
            uisrv->cmd[--uisrv->cmdlen] = '\0';
            Print(&uisrv->tty, "\b \b");
        }
        break;

    default:
        if (uisrv->cmdlen == CMD_MAXLEN)
            break;
        if (keypress < ' ' || keypress > '~') {
            if (keypress == '\t')
                keypress = ' '; /* treat tab as space */
            else
                break; /* ignore non-tab control characters */
        }
        uisrv->cmd[uisrv->cmdlen++] = keypress;
        uisrv->cmd[uisrv->cmdlen]   = '\0';
        Putc(&uisrv->tty, keypress);
        break;
    }
}

static void
uisrv_runcmd(struct uisrv *uisrv)
{
    char *tokens[CMD_MAXTOKS];
    int ntokens;

    Print(&uisrv->tty,
        TERM_FORCE_CURSOR(STR(CMD_ROW), STR(CMD_COL))
        TERM_ERASE_EOL
        TERM_SAVE_CURSOR
        TERM_FORCE_CURSOR(STR(STATUS_ROW), STR(STATUS_COL))
        TERM_ERASE_EOL);

    /* TODO: parse command */
    ntokens = tokenize(uisrv->cmd, tokens, ARRAY_SIZE(tokens));
    if (ntokens < 0) {
        Print(&uisrv->tty, "error: too many arguments");
    } else if (ntokens == 0) {
        /* ignore silently */
    } else if (!strcmp(tokens[0], "q")) {
        uisrv_cmd_q(uisrv, &tokens[1], ntokens - 1);
    } else {
        Print(&uisrv->tty, "error: unrecognized command: ");
        Print(&uisrv->tty, tokens[0]);
    }

    Print(&uisrv->tty, TERM_RESTORE_CURSOR);
    uisrv->cmdlen = 0;
    uisrv->cmd[0] = '\0';
}

static void
uisrv_cmd_q(struct uisrv *uisrv, char *argv[], int argc)
{
    int rc;
    (void)argv;
    if (argc != 0) {
        Print(&uisrv->tty, "usage: q");
        return;
    }

    Print(&uisrv->tty, TERM_RESET_DEVICE TERM_ERASE_ALL);
    rc = Flush(&uisrv->tty);
    assertv(rc, rc == SERIAL_OK);
    rc = Delay(&uisrv->clock, 2); /* allow UART to shift out last character */
    Shutdown();
}

static void
uisrv_set_time(struct uisrv *uisrv, int tenths)
{
    unsigned min, sec;
    assert(tenths >= 0);
    sec     = tenths / 10;
    tenths %= 10;
    min     = sec / 60;
    sec    %= 60;
    Printf(&uisrv->tty,
        TERM_SAVE_CURSOR
        TERM_FORCE_CURSOR(STR(TIME_ROW), STR(TIME_COL))
        "%02d:%02u.%u"
        TERM_RESTORE_CURSOR,
        min, sec, tenths);
}

static void
uisrv_sensors(struct uisrv *uisrv, uint8_t sensors[SENSOR_BYTES])
{
    /* TODO */
    (void)uisrv;
    (void)sensors;
}

static void
kbd_listen(void)
{
    tid_t uisrv;
    struct serialctx tty;
    struct uimsg msg;
    int c, rplylen;

    uisrv = MyParentTid();
    serialctx_init(&tty, COM2);

    msg.type = UIMSG_KEYBOARD;
    for (;;) {
        c = Getc(&tty);
        assertv(c, c >= 0);
        msg.keypress = (char)c;
        rplylen = Send(uisrv, &msg, sizeof (msg), NULL, 0);
        assertv(rplylen, rplylen == 0);
    }
}

static void
time_listen(void)
{
    int now_clock, rplylen;
    struct clkctx clock;
    struct uimsg msg;
    tid_t uisrv;

    uisrv = MyParentTid();
    clkctx_init(&clock);

    now_clock       = Time(&clock);
    msg.type        = UIMSG_SET_TIME;
    msg.time_tenths = 0;
    for (;;) {
        rplylen = Send(uisrv, &msg, sizeof (msg), NULL, 0);
        assertv(rplylen, rplylen == 0);
        now_clock += 10;
        msg.time_tenths++;
        DelayUntil(&clock, now_clock);
    }
}

void uictx_init(struct uictx *ui)
{
    ui->uisrv_tid = WhoIs("ui");
}

void ui_sensors(struct uictx *ui, uint8_t new_sensors[SENSOR_BYTES])
{
    int rplylen;
    struct uimsg msg = { .type = UIMSG_SENSORS };
    memcpy(msg.sensors, new_sensors, SENSOR_BYTES);
    rplylen = Send(ui->uisrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

static int
tokenize(char *cmd, char **ts, int max_ts)
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
