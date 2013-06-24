#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "u_tid.h"
#include "static_assert.h"
#include "sensor.h"
#include "ui_srv.h"

#include "xassert.h"
#include "xdef.h"
#include "xmemcpy.h"
#include "xstring.h"
#include "array_size.h"

#include "u_syscall.h"
#include "ns.h"
#include "clock_srv.h"
#include "serial_srv.h"
#include "tcmux_srv.h"

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
#define TERM_SAVE_CURSOR            "\e[s"
#define TERM_RESTORE_CURSOR         "\e[u"
#define TERM_FORCE_CURSOR_START     "\e["
#define TERM_FORCE_CURSOR_MID       ";"
#define TERM_FORCE_CURSOR_END       "f"
#define TERM_FORCE_CURSOR(row, col) \
    TERM_FORCE_CURSOR_START         \
    row TERM_FORCE_CURSOR_MID col   \
    TERM_FORCE_CURSOR_END

/* Screen positions and messages */
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
#define SWITCHES_LBL_ROW            8
#define SWITCHES_LBL_COL            1
#define SWITCHES_LBL    \
    "  1  2  3  4  5  6  7  8  9" \
    " 10 11 12 13 14 15 16 17 18" \
    "x99x9Ax9Bx9C\n"              \
    "  S  S  S  S  S  S  S  S  S" \
    "  S  S  S  S  S  S  S  S  S" \
    "  S  S  S  S"
#define SWITCHES_ROW                9
#define SWITCHES_COL                1
#define SWITCHES_ENTRY_WIDTH        3
#define SENSORS_ROW                 11
#define SENSORS_COL                 17
#define SENSORS_LBL_ROW             11
#define SENSORS_LBL_COL             1
#define SENSORS_LBL                 "Recent sensors: "

enum {
    UIMSG_KEYBOARD,
    UIMSG_SET_TIME,
    UIMSG_SENSORS
};

struct uimsg {
    int type;
    union {
        int       time_tenths;
        char      keypress;
        sensors_t sensors[SENSOR_MODULES];
    };
};

struct uisrv {
    struct serialctx tty;
    struct clkctx    clock;
    struct tcmuxctx  tcmux;

    char    cmd[CMD_MAXLEN + 1];
    int     cmdlen;

    uint32_t sensors[MAX_SENSORS];
    int      nsensors, lastsensor;
};

static int tokenize(char *cmd, char **ts, int max_ts);
static int atou8(const char *s, uint8_t *ret);

/* FIXME */
static void uisrv_init(struct uisrv *uisrv);
static void uisrv_kbd(struct uisrv *uisrv, char keypress);
static void uisrv_runcmd(struct uisrv *uisrv);
static void uisrv_cmd_q(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_tr(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_sw(struct uisrv *uisrv, char *argv[], int argc);
static bool uisrv_update_switch_table(
    struct uisrv *uisrv, uint8_t sw, bool curved);
static void uisrv_set_time(struct uisrv *uisrv, int tenths);
static void uisrv_sensors(
    struct uisrv *uisrv, sensors_t sensors[SENSOR_MODULES]);
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
    static uint8_t switches[22] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
        0x99, 0x9a, 0x9b, 0x9c
    };
    tid_t kbd_tid;
    tid_t time_tid;
    unsigned i;
    uint8_t last_switch;

    /* Set up UI state */
    uisrv->cmdlen     = 0;
    uisrv->cmd[0]     = '\0';
    uisrv->nsensors   = 0;
    uisrv->lastsensor = MAX_SENSORS - 1;

    /* Get contexts */
    clkctx_init(&uisrv->clock);
    serialctx_init(&uisrv->tty, COM2);
    tcmuxctx_init(&uisrv->tcmux);

    /* Print initialization message */
    Print(&uisrv->tty, TERM_RESET_DEVICE TERM_ERASE_ALL "Initializating...");
    Flush(&uisrv->tty);

    /* Set all switches to straight. */
    for (i = 0; i < ARRAY_SIZE(switches) - 1; i++)
        tcmux_switch_curve(&uisrv->tcmux, switches[i], false);

    last_switch = switches[ARRAY_SIZE(switches) - 1];
    tcmux_switch_curve_sync(&uisrv->tcmux, last_switch, false);

    /* Print static screen portions */
    Print(&uisrv->tty,
        TERM_RESET_DEVICE
        TERM_ERASE_ALL
        TERM_FORCE_CURSOR(STR(TIME_ROW), STR(TIME_MSG_COL))
        TIME_MSG
        TERM_FORCE_CURSOR(STR(SWITCHES_LBL_ROW), STR(SWITCHES_LBL_COL))
        SWITCHES_LBL
        TERM_FORCE_CURSOR(STR(SENSORS_LBL_ROW), STR(SENSORS_LBL_COL))
        SENSORS_LBL
        TERM_FORCE_CURSOR(STR(CMD_ROW), STR(CMD_PROMPT_COL))
        CMD_PROMPT
        TERM_FORCE_CURSOR(STR(CMD_ROW), STR(CMD_COL)));

    Flush(&uisrv->tty);

    /* Start listeners */
    kbd_tid = Create(PRIORITY_UI - 1, &kbd_listen);
    assertv(kbd_tid, kbd_tid >= 0);

    time_tid = Create(PRIORITY_UI - 1, &time_listen);
    assertv(time_tid, time_tid >= 0);
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
    } else if (!strcmp(tokens[0], "tr")) {
        uisrv_cmd_tr(uisrv, &tokens[1], ntokens - 1);
    } else if (!strcmp(tokens[0], "sw")) {
        uisrv_cmd_sw(uisrv, &tokens[1], ntokens - 1);
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
uisrv_cmd_tr(struct uisrv *uisrv, char *argv[], int argc)
{
    /* Parse arguments */
    uint8_t which;
    uint8_t speed;
    if (argc != 2) {
        Print(&uisrv->tty, "usage: tr TRAIN SPEED");
        return;
    }
    if (atou8(argv[0], &which) != 0) {
        Printf(&uisrv->tty, "bad train '%s'", argv[0]);
        return;
    }
    if (atou8(argv[1], &speed) != 0 || speed > 14) {
        Printf(&uisrv->tty, "bad speed '%s'", argv[1]);
        return;
    }

    /* Send out speed command */
    tcmux_train_speed(&uisrv->tcmux, which, speed);
}

static void
uisrv_cmd_sw(struct uisrv *uisrv, char *argv[], int argc)
{
    /* Parse arguments */
    uint8_t sw;
    bool    curved;
    bool    bad_dir;
    if (argc != 2) {
        Print(&uisrv->tty, "usage: sw SWITCH (C|S)");
        return;
    }
    if (atou8(argv[0], &sw) != 0) {
        Printf(&uisrv->tty, "bad switch '%s'", argv[0]);
        return;
    }

    bad_dir = false;
    if (argv[1][0] == '\0' || argv[1][1] != '\0')
        bad_dir = true;
    else if (argv[1][0] == 'c' || argv[1][0] == 'C')
        curved = true;
    else if (argv[1][0] == 's' || argv[1][0] == 'S')
        curved = false;
    else
        bad_dir = true;

    if (bad_dir) {
        Printf(&uisrv->tty, "bad direction '%s'", argv[1]);
        return;
    }

    /* Send out switch command */
    if (!uisrv_update_switch_table(uisrv, sw, curved)) {
        Printf(&uisrv->tty, "unrecognized switch '%s'", argv[0]);
        return;
    }

    tcmux_switch_curve(&uisrv->tcmux, sw, curved);
}

static bool
uisrv_update_switch_table(struct uisrv *uisrv, uint8_t sw, bool curved)
{
    int col;
    if (sw >= 0x99 && sw <= 0x9c)
        sw -= 0x99 - 19;
    else if (sw < 1 || sw > 18)
        return false;

    /* Update displayed switch table */
    col = SWITCHES_COL + (SWITCHES_ENTRY_WIDTH * sw - 1);
    Printf(&uisrv->tty,
        TERM_FORCE_CURSOR(STR(SWITCHES_ROW), "%d")
        "%c",
        col,
        curved ? 'C' : 'S');
    return true;
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
uisrv_sensors(struct uisrv *uisrv, sensors_t sensors[SENSOR_MODULES])
{
    int i, j, bit;
    sensors_t sensor;
    bool change;

    change = false;
    for (i = 0; i < SENSOR_MODULES; i++) {
        sensor = sensors[i];
        if (sensor == 0)
            continue;

        change = true;
        for (j = 0, bit = 1; j < SENSORS_PER_MODULE; j++, bit <<= 1) {
            if ((sensor & bit) == 0)
                continue;
            uisrv->lastsensor++;
            uisrv->lastsensor %= MAX_SENSORS;
            uisrv->sensors[uisrv->lastsensor] = (i << 16) | j;
            if (uisrv->nsensors < MAX_SENSORS)
                uisrv->nsensors++;
        }
    }

    if (!change)
        return;

    /* The most recent sensor list changed. Update it on screen */
    Print(&uisrv->tty,
        TERM_SAVE_CURSOR
        TERM_FORCE_CURSOR(STR(SENSORS_ROW), STR(SENSORS_COL))
        TERM_ERASE_EOL);

    j = uisrv->lastsensor;
    for (i = 0; i < uisrv->nsensors; i++) {
        uint32_t sens = uisrv->sensors[j];
        Printf(&uisrv->tty, "%c%d ", (sens >> 16) + 'A', (sens & 0xffff) + 1);
        if (--j < 0)
            j += MAX_SENSORS;
    }

    Print(&uisrv->tty, TERM_RESTORE_CURSOR);
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

void ui_sensors(struct uictx *ui, sensors_t new_sensors[SENSOR_MODULES])
{
    int rplylen;
    struct uimsg msg = { .type = UIMSG_SENSORS };
    memcpy(msg.sensors, new_sensors, SENSOR_MODULES * sizeof (sensors_t));
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

static int
atou8(const char *s, uint8_t *ret)
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
