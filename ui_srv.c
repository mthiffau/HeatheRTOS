#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "u_tid.h"
#include "static_assert.h"
#include "sensor.h"
#include "ui_srv.h"

#include "xassert.h"
#include "xarg.h"
#include "xmemcpy.h"
#include "xstring.h"
#include "ringbuf.h"
#include "array_size.h"
#include "track_graph.h"

#include "u_syscall.h"
#include "ns.h"
#include "clock_srv.h"
#include "serial_srv.h"
#include "tcmux_srv.h"
#include "sensor_srv.h"
#include "switch_srv.h"
#include "train_srv.h"
#include "dbglog_srv.h"

#include "bwio.h"

#define STR(x)              STR1(x)
#define STR1(x)             #x

#define CMD_MAXLEN          63
#define CMD_MAXTOKS         16
#define MAX_SENSORS         10

#define MAX_TRAINS          256
#define REVERSE_DELAY_TICKS 200
#define REVERSE_CMD         15

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

#define TERM_SCROLL_DOWN            "\eD"
#define TERM_SCROLL_UP              "\eM"
#define TERM_SETSCROLL_START        "\e["
#define TERM_SETSCROLL_MID          ";"
#define TERM_SETSCROLL_END          "r"
#define TERM_SETSCROLL(a, b)        \
    TERM_SETSCROLL_START            \
    a TERM_SETSCROLL_MID b          \
    TERM_SETSCROLL_END

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
#define DBGLOG_TOP_ROW              15
#define DBGLOG_BTM_ROW              30
#define DBGLOG_COL                  1
#define TRAINPOS_ROW                13
#define TRAINPOS_COL                17
#define TRAINPOS_LBL_ROW            13
#define TRAINPOS_LBL_COL            1
#define TRAINPOS_LBL                "Train position: "

enum {
    UIMSG_KEYBOARD,
    UIMSG_SET_TIME,
    UIMSG_SENSORS,
    UIMSG_DBGLOG,
    UIMSG_SWITCH_UPDATE,
    UIMSG_TRAINPOS_UPDATE,
    UIMSG_REVERSE,
    UIMSG_REVERSE_DONE,
};

struct train {
    uint8_t speed;

    bool    reversing;      /* Is the train reversing */
    int     rv_start_time;  /* When did it start reversing */
    int     reverse_count;  /* How many concurrent reversals */

    int     next;           /* Next index if we're in a queue */

    struct trainctx task;
    bool            running;
};

struct rv_msg {
    uint8_t train;
    int     time;
};

struct uimsg {
    int type;
    union {
        int           time_tenths;
        char          keypress;
        struct rv_msg rv;
        sensors_t     sensors[SENSOR_MODULES];
        struct {
            uint8_t   sw;
            bool      curved;
        } swupdate;
        struct {
            int       len;
            char      buf[32];
        } dbglog;
        struct trainest trainest;
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

    struct train traintab[MAX_TRAINS];

    tid_t reverse_timer;
    bool  reverse_timer_running;

    int   rvq_head;
    int   rvq_tail;
    int   rvq_count;

    struct trainest           trainest_last;
    const struct track_graph *track;

    struct ringbuf dbglog;
    char           dbglog_mem[4096];
    int            n_loglines;
};

static int tokenize(char *cmd, char **ts, int max_ts);
static int atou8(const char *s, uint8_t *ret);

/* FIXME */
static void uisrv_init(struct uisrv *uisrv);
static void uisrv_kbd(struct uisrv *uisrv, char keypress);
static void uisrv_runcmd(struct uisrv *uisrv);
static void uisrv_cmd_q(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_track(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_addtrain(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_goto(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_speed(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_stop(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_vcalib(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_stopcalib(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_orient(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_tr(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_sw(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_rv(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_lt(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_path(struct uisrv *uisrv, char *argv[], int argc);
static void start_reverse_timer(struct uisrv *uisrv);
static bool uisrv_update_switch_table(
    struct uisrv *uisrv, uint8_t sw, bool curved);
static void uisrv_trainpos_update(struct uisrv *uisrv, struct trainest *est);
static void uisrv_set_time(struct uisrv *uisrv, int tenths);
static void uisrv_sensors(
    struct uisrv *uisrv, sensors_t sensors[SENSOR_MODULES]);
static void uisrv_dbglog(struct uisrv *uisrv, char *buf, int len);
static void uisrv_finish_reverse(struct uisrv *uisrv);
static const struct track_node* uisrv_lookup_node(
    struct uisrv *uisrv, const char *name);
static void kbd_listen(void);
static void time_listen(void);
static void sensor_listen(void);
static void switch_listen(void);
static void trainpos_listen(void);
static void dbglog_listen(void);
static void reverse_timer(void);

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
        case UIMSG_SWITCH_UPDATE:
            uisrv_update_switch_table(
                &uisrv,
                msg.swupdate.sw,
                msg.swupdate.curved);
            break;
        case UIMSG_TRAINPOS_UPDATE:
            uisrv_trainpos_update(&uisrv, &msg.trainest);
            break;
        case UIMSG_DBGLOG:
            uisrv_dbglog(&uisrv, msg.dbglog.buf, msg.dbglog.len);
            break;
        case UIMSG_REVERSE_DONE:
            uisrv_finish_reverse(&uisrv);
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
    tid_t kbd_tid, time_tid, sensor_tid, switch_tid, dbglog_tid;
    unsigned i;
    uint8_t last_switch;

    uisrv->track = NULL;
    rbuf_init(&uisrv->dbglog, uisrv->dbglog_mem, sizeof (uisrv->dbglog_mem));
    uisrv->n_loglines = 0;

    /* Set up UI state */
    uisrv->cmdlen     = 0;
    uisrv->cmd[0]     = '\0';
    uisrv->nsensors   = 0;
    uisrv->lastsensor = MAX_SENSORS - 1;

    /* Get contexts */
    clkctx_init(&uisrv->clock);
    serialctx_init(&uisrv->tty, COM2);
    tcmuxctx_init(&uisrv->tcmux);

    /* Initialize train table */
    for (i = 0; i < MAX_TRAINS; i++) {
        uisrv->traintab[i].speed         = 0;
        uisrv->traintab[i].reversing     = false;
        uisrv->traintab[i].rv_start_time = -1;
        uisrv->traintab[i].reverse_count = 0;
        uisrv->traintab[i].next          = -1;
        uisrv->traintab[i].running       = false;
    }

    /* Print initialization message */
    Print(&uisrv->tty, TERM_RESET_DEVICE TERM_ERASE_ALL "Initializing...");
    Flush(&uisrv->tty);

    /* Set all switches to straight. */
    for (i = 0; i < ARRAY_SIZE(switches) - 1; i++)
        tcmux_switch_curve(&uisrv->tcmux, switches[i], false);

    last_switch = switches[ARRAY_SIZE(switches) - 1];
    tcmux_switch_curve_sync(&uisrv->tcmux, last_switch, false);

    /* Print static screen portions */
    Print(&uisrv->tty, TERM_RESET_DEVICE TERM_ERASE_ALL);
    Flush(&uisrv->tty);
    Delay(&uisrv->clock, 2);
    Print(&uisrv->tty,
        TERM_FORCE_CURSOR(STR(TIME_ROW), STR(TIME_MSG_COL))
        TIME_MSG);
    Flush(&uisrv->tty);
    Delay(&uisrv->clock, 2);
    Print(&uisrv->tty,
        TERM_FORCE_CURSOR(STR(SWITCHES_LBL_ROW), STR(SWITCHES_LBL_COL))
        SWITCHES_LBL);
    Flush(&uisrv->tty);
    Delay(&uisrv->clock, 2);
    Print(&uisrv->tty,
        TERM_FORCE_CURSOR(STR(SENSORS_LBL_ROW), STR(SENSORS_LBL_COL))
        SENSORS_LBL);
    Flush(&uisrv->tty);
    Delay(&uisrv->clock, 2);
    Print(&uisrv->tty,
        TERM_FORCE_CURSOR(STR(TRAINPOS_LBL_ROW), STR(TRAINPOS_LBL_COL))
        TRAINPOS_LBL);
    Flush(&uisrv->tty);
    Delay(&uisrv->clock, 2);
    Print(&uisrv->tty,
        TERM_SETSCROLL(STR(DBGLOG_TOP_ROW), STR(DBGLOG_BTM_ROW)));
    Flush(&uisrv->tty);
    Delay(&uisrv->clock, 2);
    Print(&uisrv->tty,
        TERM_FORCE_CURSOR(STR(CMD_ROW), STR(CMD_PROMPT_COL))
        CMD_PROMPT);
    Flush(&uisrv->tty);
    Delay(&uisrv->clock, 2);
    Print(&uisrv->tty,
        TERM_FORCE_CURSOR(STR(CMD_ROW), STR(CMD_COL)));
    Flush(&uisrv->tty);
    Delay(&uisrv->clock, 2);

    /* Start listeners */
    kbd_tid = Create(PRIORITY_UI - 1, &kbd_listen);
    assertv(kbd_tid, kbd_tid >= 0);

    time_tid = Create(PRIORITY_UI - 1, &time_listen);
    assertv(time_tid, time_tid >= 0);

    sensor_tid = Create(PRIORITY_UI - 1, &sensor_listen);
    assertv(sensor_tid, sensor_tid >= 0);

    switch_tid = Create(PRIORITY_UI - 1, &switch_listen);
    assertv(switch_tid, switch_tid >= 0);

    dbglog_tid = Create(PRIORITY_UI - 1, &dbglog_listen);
    assertv(dblog_tid, dbglog_tid >= 0);

    uisrv->reverse_timer = Create(PRIORITY_UI - 1, &reverse_timer);
    uisrv->reverse_timer_running = false;
    uisrv->rvq_head = -1;
    uisrv->rvq_tail = -1;
    uisrv->rvq_count = 0;
    assert(uisrv->reverse_timer >= 0);
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
    } else if (!strcmp(tokens[0], "track")) {
        uisrv_cmd_track(uisrv, &tokens[1], ntokens - 1);
    } else if (!strcmp(tokens[0], "addtrain")) {
        uisrv_cmd_addtrain(uisrv, &tokens[1], ntokens - 1);
    } else if (!strcmp(tokens[0], "goto")) {
        uisrv_cmd_goto(uisrv, &tokens[1], ntokens - 1);
    } else if (!strcmp(tokens[0], "speed")) {
        uisrv_cmd_speed(uisrv, &tokens[1], ntokens - 1);
    } else if (!strcmp(tokens[0], "stop")) {
        uisrv_cmd_stop(uisrv, &tokens[1], ntokens - 1);
    } else if (!strcmp(tokens[0], "vcalib")) {
        uisrv_cmd_vcalib(uisrv, &tokens[1], ntokens - 1);
    } else if (!strcmp(tokens[0], "stopcalib")) {
        uisrv_cmd_stopcalib(uisrv, &tokens[1], ntokens - 1);
    } else if (!strcmp(tokens[0], "orient")) {
        uisrv_cmd_orient(uisrv, &tokens[1], ntokens - 1);
    } else if (!strcmp(tokens[0], "tr!")) {
        uisrv_cmd_tr(uisrv, &tokens[1], ntokens - 1);
    } else if (!strcmp(tokens[0], "sw!")) {
        uisrv_cmd_sw(uisrv, &tokens[1], ntokens - 1);
    } else if (!strcmp(tokens[0], "rv!")) {
        uisrv_cmd_rv(uisrv, &tokens[1], ntokens - 1);
    } else if (!strcmp(tokens[0], "lt!")) {
        uisrv_cmd_lt(uisrv, &tokens[1], ntokens - 1);
    } else if (!strcmp(tokens[0], "path")) {
        uisrv_cmd_path(uisrv, &tokens[1], ntokens - 1);
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
uisrv_cmd_track(struct uisrv *uisrv, char *argv[], int argc)
{
    int track;
    if (argc != 1) {
        Print(&uisrv->tty, "usage: track (a|b)");
        return;
    }

    if (!strcmp(argv[0], "a")) {
        track = 0;
    } else if (!strcmp(argv[0], "b")) {
        track = 1;
    } else {
        Print(&uisrv->tty, "usage: track (a|b)");
        return;
    }

    if (uisrv->track != NULL) {
        Print(&uisrv->tty, "track already selected");
        return;
    }

    uisrv->track = all_tracks[track];
}

static void
uisrv_cmd_addtrain(struct uisrv *uisrv, char *argv[], int argc)
{
    struct traincfg cfg;
    int rplylen;
    tid_t tid;

    if (uisrv->track == NULL) {
        Print(&uisrv->tty, "no track selected");
        return;
    }

    if (argc != 1) {
        Print(&uisrv->tty, "usage: addtrain TRAIN");
        return;
    }

    for (cfg.track_id = 0; cfg.track_id < TRACK_COUNT; cfg.track_id++) {
        if (all_tracks[cfg.track_id] == uisrv->track)
            break;
    }
    assert(all_tracks[cfg.track_id] == uisrv->track);

    if (atou8(argv[0], &cfg.train_id) != 0) {
        Printf(&uisrv->tty, "bad train '%s'", argv[0]);
        return;
    }

    /* Start train server for the requested train */
    tid = Create(PRIORITY_TRAIN, &trainsrv_main);
    assertv(tid, tid >= 0);
    rplylen = Send(tid, &cfg, sizeof (cfg), NULL, 0);
    assertv(rplylen, rplylen == 0);

    tid = Create(PRIORITY_UI - 1, &trainpos_listen);
    assertv(tid, tid >= 0);
    rplylen = Send(tid, &cfg, sizeof (cfg), NULL, 0);
    assertv(rplylen, rplylen == 0);

    uisrv->traintab[cfg.train_id].running = true;
    trainctx_init(
        &uisrv->traintab[cfg.train_id].task,
        uisrv->track,
        cfg.train_id);
}

static void
uisrv_cmd_goto(struct uisrv *uisrv, char *argv[], int argc)
{
    uint8_t train;
    const struct track_node *dest;

    if (uisrv->track == NULL) {
        Print(&uisrv->tty, "no track selected");
        return;
    }

    if (argc != 2) {
        Print(&uisrv->tty, "usage: goto TRAIN NODE");
        return;
    }

    if (atou8(argv[0], &train) != 0) {
        Printf(&uisrv->tty, "bad train '%s'", argv[0]);
        return;
    }

    dest = uisrv_lookup_node(uisrv, argv[1]);
    if (dest == NULL) {
        Printf(&uisrv->tty, "unknown node '%s'", argv[1]);
        return;
    }

    if (!uisrv->traintab[train].running) {
        Printf(&uisrv->tty, "train %d not active", train);
        return;
    }

    train_moveto(&uisrv->traintab[train].task, dest);
}

static void
uisrv_cmd_speed(struct uisrv *uisrv, char *argv[], int argc)
{
    uint8_t train;
    uint8_t speed;

    if (uisrv->track == NULL) {
        Print(&uisrv->tty, "no track selected");
        return;
    }

    if (argc != 2) {
        Print(&uisrv->tty, "usage: speed TRAIN SPEED");
        return;
    }

    if (atou8(argv[0], &train) != 0) {
        Printf(&uisrv->tty, "bad train '%s'", argv[0]);
        return;
    }

    if (atou8(argv[1], &speed) != 0 || speed <= 0 || speed >= 15) {
        Printf(&uisrv->tty, "bad speed '%s'", argv[1]);
        return;
    }

    if (!uisrv->traintab[train].running) {
        Printf(&uisrv->tty, "train %d not active", train);
        return;
    }

    train_setspeed(&uisrv->traintab[train].task, speed);
}

static void
uisrv_cmd_stop(struct uisrv *uisrv, char *argv[], int argc)
{
    uint8_t train;
    if (argc != 1) {
        Print(&uisrv->tty, "usage: stop TRAIN");
        return;
    }

    if (atou8(argv[0], &train) != 0) {
        Printf(&uisrv->tty, "bad train '%s'", argv[0]);
        return;
    }

    if (!uisrv->traintab[train].running) {
        Printf(&uisrv->tty, "train %d not active", train);
        return;
    }

    train_stop(&uisrv->traintab[train].task);
}

static void
uisrv_cmd_vcalib(struct uisrv *uisrv, char *argv[], int argc)
{
    uint8_t train, minspeed, maxspeed;
    if (argc != 3) {
        Print(&uisrv->tty, "usage: vcalib TRAIN MINSPEED MAXSPEED");
        return;
    }

    if (atou8(argv[0], &train) != 0) {
        Printf(&uisrv->tty, "bad train '%s'", argv[0]);
        return;
    }

    if (atou8(argv[1], &minspeed) != 0 || minspeed > 14) {
        Printf(&uisrv->tty, "bad speed '%s'", argv[1]);
        return;
    }

    if (atou8(argv[2], &maxspeed) != 0 || maxspeed > 14) {
        Printf(&uisrv->tty, "bad speed '%s'", argv[2]);
        return;
    }

    if (!uisrv->traintab[train].running) {
        Printf(&uisrv->tty, "train %d not active", train);
        return;
    }

    train_vcalib(&uisrv->traintab[train].task, minspeed, maxspeed);
}

static void
uisrv_cmd_stopcalib(struct uisrv *uisrv, char *argv[], int argc)
{
    uint8_t train;
    uint8_t speed;
    if (argc != 2) {
        Print(&uisrv->tty, "usage: stopcalib TRAIN SPEED");
        return;
    }

    if (atou8(argv[0], &train) != 0) {
        Printf(&uisrv->tty, "bad train '%s'", argv[0]);
        return;
    }

    if (!uisrv->traintab[train].running) {
        Printf(&uisrv->tty, "train %d not active", train);
        return;
    }

    if (atou8(argv[1], &speed) != 0 || speed <= 0 || speed >= 15) {
        Printf(&uisrv->tty, "bad speed '%s'", argv[1]);
        return;
    }

    train_stopcalib(&uisrv->traintab[train].task, speed);
}

static void
uisrv_cmd_orient(struct uisrv *uisrv, char *argv[], int argc)
{
    uint8_t train;
    if (argc != 1) {
        Print(&uisrv->tty, "usage: orient TRAIN");
        return;
    }

    if (atou8(argv[0], &train) != 0) {
        Printf(&uisrv->tty, "bad train '%s'", argv[0]);
        return;
    }

    if (!uisrv->traintab[train].running) {
        Printf(&uisrv->tty, "train %d not active", train);
        return;
    }

    train_orient(&uisrv->traintab[train].task);
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

    if (!uisrv->traintab[which].reversing) {
        /* Send out speed command */
        tcmux_train_speed(&uisrv->tcmux, which, speed);
    }
    /* Store state in train table */
    uisrv->traintab[which].speed = speed;
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
    tcmux_switch_curve(&uisrv->tcmux, sw, curved);
}

static void
uisrv_cmd_rv(struct uisrv *uisrv, char *argv[], int argc)
{
    uint8_t which;
    struct train* train;

    /* Parse arguments */
    if (argc != 1) {
        Print(&uisrv->tty, "usage: rv TRAIN");
        return;
    }
    if (atou8(argv[0], &which) != 0) {
        Printf(&uisrv->tty, "bad train '%s'", argv[0]);
        return;
    }

    train = &(uisrv->traintab[which]);

    /* If the train is already reversing,
       just increment it's reverse count */
    if (train->reversing) {
        train->reverse_count++;
        return;
    }

    /* Mark the train as reversing */
    train->reversing = true;
    train->rv_start_time = Time(&uisrv->clock);
    train->reverse_count = 1;

    /* Immediately stop the train */
    tcmux_train_speed(&uisrv->tcmux, which, 0);

    /* Add the train to the reverse queue */
    if (uisrv->rvq_count == 0) {
        uisrv->rvq_head = which;
        uisrv->rvq_tail = which;
    } else {
        uisrv->traintab[uisrv->rvq_tail].next = which;
        uisrv->rvq_tail = which;
    }
    uisrv->rvq_count++;

    /* If the reverse timer isn't running, send message to the reverse timer */
    start_reverse_timer(uisrv);
}

static void
uisrv_cmd_lt(struct uisrv *uisrv, char *argv[], int argc)
{
    /* Parse arguments */
    uint8_t which;
    bool    light;
    int     i;
    if (argc != 2) {
        Print(&uisrv->tty, "usage: lt TRAIN (on|off)");
        return;
    }
    if (atou8(argv[0], &which) != 0) {
        Printf(&uisrv->tty, "bad train '%s'", argv[0]);
        return;
    }
    /* convert second argument to lower case */
    for (i = 0; argv[1][i] != '\0'; i++) {
        if (argv[1][i] >= 'A' && argv[1][i] <= 'Z')
            argv[1][i] += 'a' - 'A';
    }
    if (strcmp(argv[1], "on") == 0) {
        light = true;
    } else if (strcmp(argv[1], "off") == 0) {
        light = false;
    } else {
        Print(&uisrv->tty, "usage: lt TRAIN (on|off)");
        return;
    }

    /* Send light command */
    tcmux_train_light(&uisrv->tcmux, which, light);
}

static void
uisrv_cmd_path(struct uisrv *uisrv, char *argv[], int argc)
{
    /* Parse arguments */
    const struct track_node  *nodes[2];
    struct track_path         path;
    int                       rc;
    unsigned                  i;

    if (uisrv->track == NULL) {
        Print(&uisrv->tty, "no track selected");
        return;
    }

    if (argc != 2) {
        Print(&uisrv->tty, "usage: path START STOP");
        return;
    }

    for (i = 0; i < 2; i++) {
        nodes[i] = uisrv_lookup_node(uisrv, argv[i]);
        if (nodes[i] == NULL) {
            Printf(&uisrv->tty, "error: unknown node %s", argv[i]);
            return;
        }
    }

    /* Find the path! */
    rc = track_pathfind(uisrv->track, nodes[0], nodes[1], &path);
    if (rc == -1) {
        Print(&uisrv->tty, "no (directed) path");
    } else {
        for (i = 0; i < path.hops; i++) {
            Printf(&uisrv->tty, "%s ", path.edges[i]->src->name);
        }
    }
}

static const struct track_node*
uisrv_lookup_node(struct uisrv *uisrv, const char *name)
{
    int j;
    assert(uisrv->track != NULL);
    for (j = 0; j < uisrv->track->n_nodes; j++) {
        if (!strcmp(name, uisrv->track->nodes[j].name))
            return &uisrv->track->nodes[j];
    }
    return NULL;
}

static void
start_reverse_timer(struct uisrv *uisrv)
{
    int replylen;
    struct uimsg msg;
    struct train *train;

    assert(uisrv->rvq_count > 0);
    if (uisrv->reverse_timer_running)
        return;

    train = &uisrv->traintab[uisrv->rvq_head];

    msg.type     = UIMSG_REVERSE;
    msg.rv.train = uisrv->rvq_head;
    msg.rv.time  = train->rv_start_time + REVERSE_DELAY_TICKS;

    uisrv->reverse_timer_running = true;
    replylen = Send(uisrv->reverse_timer, &msg, sizeof(msg), NULL, 0);
    assertv(replylen, replylen == 0);
}

static void
uisrv_trainpos_update(struct uisrv *uisrv, struct trainest *est)
{
    /*
    bool big_change;
    int dx;

    dx = est->ahead_mm - uisrv->trainest_last.ahead_mm;
    if (dx < 0)
        dx = -dx;

    big_change = dx > 50
        || est->ahead    != uisrv->trainest_last.ahead
        || est->lastsens != uisrv->trainest_last.lastsens
        || est->err_mm   != uisrv->trainest_last.err_mm;

    if (!big_change)
        return;
        */

    /* Find log2 of ahead_mm */
    /*
    static const int MultiplyDeBruijnBitPosition[32] =
    {
          0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
            8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
    };

    log_mm = est->ahead_mm;

    log_mm |= log_mm >> 1; // first round down to one less than a power of 2 
    log_mm |= log_mm >> 2;
    log_mm |= log_mm >> 4;
    log_mm |= log_mm >> 8;
    log_mm |= log_mm >> 16;

    log_mm = MultiplyDeBruijnBitPosition[(uint32_t)(v * 0x07C4ACDDU) >> 27];
    // done finding log */

    Printf(&uisrv->tty,
        TERM_SAVE_CURSOR
        TERM_FORCE_CURSOR(STR(TRAINPOS_ROW), STR(TRAINPOS_COL))
        //TERM_ERASE_EOL
        "T %02d: %5s-%04dmm",
        est->train_id,
        est->ahead->dest->name,
        est->ahead_mm);

    if (est->lastsens != NULL) {
        Printf(&uisrv->tty, ", err %04dmm @ %s",
            est->err_mm,
            est->lastsens->name);
    }

    Print(&uisrv->tty, TERM_RESTORE_CURSOR);
    memcpy(&uisrv->trainest_last, est, sizeof (*est));
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
        TERM_SAVE_CURSOR
        TERM_FORCE_CURSOR(STR(SWITCHES_ROW), "%d")
        "%c"
        TERM_RESTORE_CURSOR,
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
uisrv_dbglog(struct uisrv *uisrv, char *buf, int len)
{
    int i;

    rbuf_write(&uisrv->dbglog, buf, len);
    for (i = 0; i < len; i++) {
        if (buf[i] == '\n')
            uisrv->n_loglines++;
    }

    if (uisrv->n_loglines == 0)
        return;

    Print(&uisrv->tty,
        TERM_SAVE_CURSOR
        TERM_FORCE_CURSOR(STR(DBGLOG_BTM_ROW), STR(DBGLOG_COL)));
    while (uisrv->n_loglines > 0) {
        char write_buf[32];
        char c;
        int max = sizeof (write_buf);
        len = 0;
        while (len < max && rbuf_getc(&uisrv->dbglog, &c)) {
            write_buf[len++] = c;
            if (c == '\n' && --uisrv->n_loglines == 0)
                break;
        }
        Write(&uisrv->tty, write_buf, len);
    }
    Print(&uisrv->tty, TERM_RESTORE_CURSOR);
}

static void
uisrv_finish_reverse(struct uisrv *uisrv)
{
    uisrv->reverse_timer_running = false;

    /* Get the head of the reverse queue */
    int train_ix = uisrv->rvq_head;
    assert(train_ix > 0);
    struct train *train = &(uisrv->traintab[train_ix]);

    /* If it turns out we are reversing, send the reverse command */
    if (train->reverse_count % 2 == 1)
        tcmux_train_speed(&uisrv->tcmux, train_ix, REVERSE_CMD);
    tcmux_train_speed(&uisrv->tcmux, train_ix, train->speed);

    /* The train isn't reversing any more */
    train->reversing = false;
    train->reverse_count = 0;

    /* Take it off the queue */
    uisrv->rvq_head = train->next;
    uisrv->rvq_count--;
    train->next = -1;

    /* If the queue is empty now, we're done */
    if (uisrv->rvq_count == 0) {
        uisrv->rvq_tail = -1;
        return;
    }

    start_reverse_timer(uisrv);
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

static void
switch_listen(void)
{
    struct switchctx switches;
    struct uimsg msg;
    tid_t uisrv;

    uisrv = MyParentTid();
    switchctx_init(&switches);

    msg.type = UIMSG_SWITCH_UPDATE;
    for (;;) {
        int rplylen;
        msg.swupdate.sw = switch_wait(&switches, &msg.swupdate.curved);
        rplylen = Send(uisrv, &msg, sizeof (msg), NULL, 0);
        assertv(rplylen, rplylen == 0);
    }
}

static void
trainpos_listen(void)
{
    struct trainctx train;
    struct traincfg cfg;
    struct uimsg msg;
    int rc, msglen;
    tid_t uisrv;

    /* Get config */
    msglen = Receive(&uisrv, &cfg, sizeof (cfg));
    assertv(msglen, msglen == sizeof (cfg));
    rc = Reply(uisrv, NULL, 0);
    assertv(rc, rc == 0);

    /* Setup train context */
    trainctx_init(&train, all_tracks[cfg.track_id], cfg.train_id);

    msg.type = UIMSG_TRAINPOS_UPDATE;
    for (;;) {
        int rplylen;
        train_estimate(&train, &msg.trainest);
        rplylen = Send(uisrv, &msg, sizeof (msg), NULL, 0);
        assertv(rplylen, rplylen == 0);
    }
}

static void
sensor_listen(void)
{
    struct sensorctx sens;
    struct uictx ui;
    int rc;
    uictx_init(&ui);
    sensorctx_init(&sens);
    for (;;) {
        sensors_t sensors[SENSOR_MODULES];
        unsigned i;
        for (i = 0; i < ARRAY_SIZE(sensors); i++)
            sensors[i] = (sensors_t)-1;

        rc = sensor_wait(&sens, sensors, -1, NULL);
        assertv(rc, rc == SENSOR_TRIPPED);
        ui_sensors(&ui, sensors);
    }
}

static void
dbglog_listen(void)
{
    struct dbglogctx dbglog;
    struct uimsg msg;
    tid_t uisrv;
    int rplylen;

    uisrv = MyParentTid();
    dbglogctx_init(&dbglog);

    msg.type = UIMSG_DBGLOG;
    for (;;) {
        msg.dbglog.len = dbglog_read(
            &dbglog,
            msg.dbglog.buf,
            sizeof (msg.dbglog.buf));
        assert(msg.dbglog.len > 0);
        rplylen = Send(uisrv, &msg, sizeof (msg), NULL, 0);
        assertv(rplylen, rplylen == 0);
    }
}

static void
reverse_timer(void)
{
    int rc;
    tid_t uisrv;
    struct uimsg msg;
    struct clkctx clock;

    clkctx_init(&clock);

    for (;;) {
        /* Receive a reversal message from UI */
        rc = Receive(&uisrv, &msg, sizeof(msg));
        assertv(rc, rc == sizeof(msg));
        assert(msg.type == UIMSG_REVERSE);
        /* Reply immediately to unblock it */
        rc = Reply(uisrv, NULL, 0);
        assertv(rc, rc == 0);

        /* Delay for the specified time */
        rc = DelayUntil(&clock, msg.rv.time);
        assertv(rc, rc == 0);

        /* Tell the UI server we're done */
        msg.type = UIMSG_REVERSE_DONE;
        rc = Send(uisrv, &msg, sizeof(msg), NULL, 0);
        assertv(rc, rc == 0);
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
