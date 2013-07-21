#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "poly.h"
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
#include "tracksel_srv.h"
#include "track_pt.h"
#include "train_srv.h"
#include "calib_srv.h"
#include "dbglog_srv.h"

#include "bwio.h"

#define STR(x)              STR1(x)
#define STR1(x)             #x

#define CMD_MAXLEN          63
#define CMD_MAXTOKS         16
#define MAX_SENSORS         10

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
#define DBGLOG_BTM_ROW              9999
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
    UIMSG_CALIB_READY,
};

struct train {
    struct trainctx task;
    bool            running;
};

struct uimsg {
    int type;
    union {
        int           time_tenths;
        char          keypress;
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

struct calibparam {
    bool    stopping; /* Use minspeed for speed. */
    uint8_t train_id, minspeed, maxspeed;
};

struct uisrv {
    struct serialctx tty;
    struct clkctx    clock;
    struct tcmuxctx  tcmux;

    char    cmd[CMD_MAXLEN + 1];
    int     cmdlen;

    uint32_t sensors[MAX_SENSORS];
    int      nsensors, lastsensor;

    struct train traintab[TRAINS_MAX];

    struct trainest trainest_last;
    track_graph_t   track;
    bool            calib_rdy, calib_ok;
    tid_t           calib_tid;

    struct ringbuf dbglog;
    char           dbglog_mem[4096];
    int            n_loglines;
};

static int tokenize(char *cmd, char **ts, int max_ts);
static int atou8(const char *s, uint8_t *ret);
static int atou32(const char *s, uint32_t *ret, uint32_t max);

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
static void uisrv_cmd_tr(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_sw(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_rv(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_lt(struct uisrv *uisrv, char *argv[], int argc);
static void uisrv_cmd_path(struct uisrv *uisrv, char *argv[], int argc);
static bool uisrv_update_switch_table(
    struct uisrv *uisrv, uint8_t sw, bool curved);
static void uisrv_trainpos_update(struct uisrv *uisrv, struct trainest *est);
static void uisrv_set_time(struct uisrv *uisrv, int tenths);
static void uisrv_sensors(
    struct uisrv *uisrv, sensors_t sensors[SENSOR_MODULES]);
static void uisrv_dbglog(struct uisrv *uisrv, char *buf, int len);
static void kbd_listen(void);
static void time_listen(void);
static void sensor_listen(void);
static void switch_listen(void);
static void trainpos_listen(void);
static void calib_listen(void);
static void dbglog_listen(void);

static void
uisrv_empty_reply(tid_t client)
{
    int rc;
    rc = Reply(client, NULL, 0);
    assertv(rc, rc == 0);
}

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

        switch (msg.type) {
        case UIMSG_KEYBOARD:
            uisrv_empty_reply(client);
            uisrv_kbd(&uisrv, msg.keypress);
            break;
        case UIMSG_SET_TIME:
            uisrv_empty_reply(client);
            uisrv_set_time(&uisrv, msg.time_tenths);
            break;
        case UIMSG_SENSORS:
            uisrv_empty_reply(client);
            uisrv_sensors(&uisrv, msg.sensors);
            break;
        case UIMSG_SWITCH_UPDATE:
            uisrv_empty_reply(client);
            uisrv_update_switch_table(
                &uisrv,
                msg.swupdate.sw,
                msg.swupdate.curved);
            break;
        case UIMSG_TRAINPOS_UPDATE:
            uisrv_empty_reply(client);
            uisrv_trainpos_update(&uisrv, &msg.trainest);
            break;
        case UIMSG_DBGLOG:
            uisrv_empty_reply(client);
            uisrv_dbglog(&uisrv, msg.dbglog.buf, msg.dbglog.len);
            break;
        case UIMSG_CALIB_READY:
            /* NO EMPTY REPLY */
            uisrv.calib_rdy = true;
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
    for (i = 0; i < TRAINS_MAX; i++)
        uisrv->traintab[i].running = false;

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

    uisrv->calib_tid = Create(PRIORITY_UI - 1, &calib_listen);
    assert(uisrv->calib_tid >= 0);

    dbglog_tid = Create(PRIORITY_UI - 1, &dbglog_listen);
    assertv(dblog_tid, dbglog_tid >= 0);

    /* Calibration state. */
    uisrv->calib_ok  = true;
    uisrv->calib_rdy = false;
}

static void
uisrv_kbd(struct uisrv *uisrv, char keypress)
{
    switch (keypress) {
    case '\n':
        /* HACK: terminals are badly behaved.
         * But we always get a carriage return when the user hits enter. */
        break;

    case '\r':
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
    if (argc != 1) {
        Print(&uisrv->tty, "usage: track NAME");
        return;
    }

    if (uisrv->track != NULL) {
        Print(&uisrv->tty, "track already selected");
        return;
    }

    uisrv->track = track_byname(argv[0]);
    if (uisrv->track == NULL)
        Printf(&uisrv->tty, "no track named %s", argv[0]);
    else
        tracksel_tell(uisrv->track);
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
    trainctx_init(&uisrv->traintab[cfg.train_id].task, cfg.train_id);

    /* Train servers running. No longer okay to run calibration. */
    uisrv->calib_ok = false;
}

static void
uisrv_cmd_goto(struct uisrv *uisrv, char *argv[], int argc)
{
    uint8_t train;
    char *node_name, *offs;
    bool have_offs, offs_neg;
    track_node_t dest_node;
    struct track_pt dest;

    if (uisrv->track == NULL) {
        Print(&uisrv->tty, "no track selected");
        return;
    }

    if (argc != 2) {
        Print(&uisrv->tty, "usage: goto TRAIN NODE([+-]\\d+)");
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

    node_name = argv[1];
    for (offs = node_name; *offs && *offs != '+' && *offs != '-'; offs++) { }
    have_offs = (bool)*offs;
    offs_neg  = *offs == '-';
    *offs++   = '\0';

    dest_node = track_node_byname(uisrv->track, node_name);
    if (dest_node == NULL) {
        Printf(&uisrv->tty, "unknown node '%s'", node_name);
        return;
    }

    if (!have_offs) {
        track_pt_from_node(dest_node, &dest);
    } else {
        uint32_t offs_mm;
        if (atou32(offs, &offs_mm, (uint32_t)-1) != 0) {
            Printf(&uisrv->tty, "bad distance %s", offs);
            return;
        }
        if (offs_neg) {
            if (dest_node->type == TRACK_NODE_MERGE) {
                Printf(&uisrv->tty, "ambiguous destination %s-%s",
                    dest_node->name,
                    offs);
                return;
            }
            if (dest_node->type == TRACK_NODE_ENTER) {
                Printf(&uisrv->tty, "nonexistent destination %s-%s",
                    dest_node->name,
                    offs);
                return;
            }
            dest.edge   = dest_node->reverse->edge[TRACK_EDGE_AHEAD].reverse;
            dest.pos_um = 1000 * offs_mm;
            if (dest.pos_um >= 1000 * dest.edge->len_mm) {
                Printf(&uisrv->tty, "distance %dmm past end of edge", offs_mm);
                return;
            }
        } else {
            if (dest_node->type == TRACK_NODE_BRANCH) {
                Printf(&uisrv->tty, "ambiguous destination %s+%s",
                    dest_node->name,
                    offs);
                return;
            }
            if (dest_node->type == TRACK_NODE_EXIT) {
                Printf(&uisrv->tty, "nonexistent destination %s+%s",
                    dest_node->name,
                    offs);
                return;
            }
            dest.edge   = &dest_node->edge[TRACK_EDGE_AHEAD];
            dest.pos_um = 1000 * (dest.edge->len_mm - offs_mm);
            if (dest.pos_um < 0) {
                Printf(&uisrv->tty, "distance %dmm past end of edge", offs_mm);
                return;
            }
        }
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
    struct calibparam cp;
    int rc;
    if (argc != 3) {
        Print(&uisrv->tty, "usage: vcalib TRAIN MINSPEED MAXSPEED");
        return;
    }

    if (atou8(argv[0], &cp.train_id) != 0) {
        Printf(&uisrv->tty, "bad train '%s'", argv[0]);
        return;
    }

    if (atou8(argv[1], &cp.minspeed) != 0 || cp.minspeed > 14) {
        Printf(&uisrv->tty, "bad speed '%s'", argv[1]);
        return;
    }

    if (atou8(argv[2], &cp.maxspeed) != 0 || cp.maxspeed > 14) {
        Printf(&uisrv->tty, "bad speed '%s'", argv[2]);
        return;
    }

    if (cp.minspeed > cp.maxspeed) {
        Print(&uisrv->tty, "empty speed range");
        return;
    }

    if (!uisrv->calib_ok || !uisrv->calib_rdy) {
        Print(&uisrv->tty, "can't calibrate now");
        return;
    }

    cp.stopping = false;
    rc = Reply(uisrv->calib_tid, &cp, sizeof (cp));
    assertv(rc, rc == 0);
    uisrv->calib_rdy = false;
}

static void
uisrv_cmd_stopcalib(struct uisrv *uisrv, char *argv[], int argc)
{
    struct calibparam cp;
    int rc;
    if (argc != 2) {
        Print(&uisrv->tty, "usage: stopcalib TRAIN SPEED");
        return;
    }

    if (atou8(argv[0], &cp.train_id) != 0) {
        Printf(&uisrv->tty, "bad train '%s'", argv[0]);
        return;
    }

    if (atou8(argv[1], &cp.minspeed) != 0
        || cp.minspeed <= 0 || cp.minspeed >= 15) {
        Printf(&uisrv->tty, "bad speed '%s'", argv[1]);
        return;
    }

    if (!uisrv->calib_ok || !uisrv->calib_rdy) {
        Print(&uisrv->tty, "can't calibrate now");
        return;
    }

    cp.stopping = true;
    rc = Reply(uisrv->calib_tid, &cp, sizeof (cp));
    assertv(rc, rc == 0);
    uisrv->calib_rdy = false;
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
    tcmux_switch_curve(&uisrv->tcmux, sw, curved);
}

static void
uisrv_cmd_rv(struct uisrv *uisrv, char *argv[], int argc)
{
    uint8_t which;

    /* Parse arguments */
    if (argc != 1) {
        Print(&uisrv->tty, "usage: rv TRAIN");
        return;
    }
    if (atou8(argv[0], &which) != 0) {
        Printf(&uisrv->tty, "bad train '%s'", argv[0]);
        return;
    }

    /* Send the reverse command */
    tcmux_train_speed(&uisrv->tcmux, which, 15);
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
    struct track_pt   points[2];
    struct track_path path;
    int               rc;
    unsigned          i;

    if (uisrv->track == NULL) {
        Print(&uisrv->tty, "no track selected");
        return;
    }

    if (argc != 2) {
        Print(&uisrv->tty, "usage: path START STOP");
        return;
    }

    for (i = 0; i < 2; i++) {
        track_node_t node = track_node_byname(uisrv->track, argv[i]);
        if (node == NULL) {
            Printf(&uisrv->tty, "error: unknown node %s", argv[i]);
            return;
        }
        track_pt_from_node(node, &points[i]);
    }

    /* Find the path! */
#if 0
    rc = track_pathfind(uisrv->track, &points[0], 1, &points[1], 1, &path);
#endif
    rc = -1;
    if (rc == -1) {
        Print(&uisrv->tty, "no (directed) path");
    } else {
        for (i = 0; i < path.hops; i++) {
            Printf(&uisrv->tty, "%s ", path.edges[i]->src->name);
        }
    }
}

static void
uisrv_trainpos_update(struct uisrv *uisrv, struct trainest *est)
{
    Printf(&uisrv->tty,
        TERM_SAVE_CURSOR
        TERM_FORCE_CURSOR(STR(TRAINPOS_ROW), STR(TRAINPOS_COL))
        //TERM_ERASE_EOL
        "%02d: %5s-%04dmm",
        est->train_id,
        est->centre.edge->dest->name,
        est->centre.pos_um / 1000);

    if (est->lastsens != NULL) {
        Printf(&uisrv->tty, ", err %04dmm @ %5s",
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
    struct clkctx clock;
    struct uimsg msg;
    int rc, msglen;
    tid_t uisrv;

    /* Get config */
    msglen = Receive(&uisrv, &cfg, sizeof (cfg));
    assertv(msglen, msglen == sizeof (cfg));
    rc = Reply(uisrv, NULL, 0);
    assertv(rc, rc == 0);

    /* Setup contexts */
    clkctx_init(&clock);
    trainctx_init(&train, cfg.train_id);

    msg.type = UIMSG_TRAINPOS_UPDATE;
    for (;;) {
        int rplylen;
        Delay(&clock, 20);
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
calib_listen(void)
{
    struct calibparam p;
    struct uimsg msg;
    tid_t uisrv;
    int rplylen;
    uisrv = MyParentTid();
    for (;;) {
        msg.type = UIMSG_CALIB_READY;
        rplylen  = Send(uisrv, &msg, sizeof (msg), &p, sizeof (p));
        assertv(rplylen, rplylen == sizeof (p));
        if (p.stopping)
            calib_stopcalib(p.train_id, p.minspeed);
        else
            calib_vcalib(p.train_id, p.minspeed, p.maxspeed);
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
    uint32_t r;
    int rc;
    rc = atou32(s, &r, 255);
    if (rc == 0)
        *ret = (uint8_t)r;
    return rc;
}

static int
atou32(const char *s, uint32_t *ret, uint32_t max)
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

        if (n > max)
            return -1;
    }

    *ret = n;
    return 0;
}
