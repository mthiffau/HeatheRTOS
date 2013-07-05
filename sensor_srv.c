#include "config.h"
#include "xint.h"
#include "u_tid.h"
#include "static_assert.h"
#include "sensor.h"
#include "sensor_srv.h"

#include "xbool.h"
#include "xdef.h"
#include "xassert.h"
#include "array_size.h"
#include "xmemcpy.h"
#include "u_syscall.h"

#include "ns.h"
#include "clock_srv.h"

#define SENSOR_AVG_DELAY_TICKS      3

enum {
    SENSMSG_WAIT,
    SENSMSG_REPORT,
};

struct sensmsg {
    int type;
    sensors_t sensors[SENSOR_MODULES];
    int       wait_timeout;
    int       trigger_time;
};

struct sensclient {
    tid_t              tid;
    sensors_t          sensors[SENSOR_MODULES];
    int                timeout;
    struct sensclient *next;
};

struct sensrv {
    struct clkctx      clock;
    struct sensclient *clients_head;
    struct sensclient  clients[MAX_TASKS];
};

static void sensrv_init(struct sensrv *srv);
static void sensrv_wait(struct sensrv *srv, tid_t client, struct sensmsg *msg);
static void sensrv_report(
    struct sensrv *srv, tid_t client, struct sensmsg *msg);

void
sensrv_main(void)
{
    struct sensrv srv;
    struct sensmsg msg;
    int rc, msglen;
    tid_t client;

    sensrv_init(&srv);
    rc = RegisterAs("sensrv");
    assertv(rc, rc == 0);

    for (;;) {
        msglen = Receive(&client, &msg, sizeof (msg));
        assertv(msglen, msglen == sizeof (msg));
        switch (msg.type) {
        case SENSMSG_WAIT:
            sensrv_wait(&srv, client, &msg);
            break;
        case SENSMSG_REPORT:
            sensrv_report(&srv, client, &msg);
            break;
        default:
            panic("invalid sensor message type %d", msg.type);
        }
    }
}

static void
sensrv_init(struct sensrv *srv)
{
    unsigned i;
    clkctx_init(&srv->clock);
    srv->clients_head = NULL;
    for (i = 0; i < ARRAY_SIZE(srv->clients); i++)
        srv->clients[i].tid = -1;
}

static void
sensrv_wait(struct sensrv *srv, tid_t client_tid, struct sensmsg *msg)
{
    struct sensclient *client;
    int now;

    now = Time(&srv->clock);
    client = &srv->clients[client_tid & 0xff];
    assert(client->tid == -1);

    client->tid = client_tid;
    memcpy(client->sensors, msg->sensors, sizeof (client->sensors));
    if (msg->wait_timeout >= 0)
        client->timeout = now + msg->wait_timeout;
    else
        client->timeout = -1;

    client->next = srv->clients_head;
    srv->clients_head = client;
}

static void
sensrv_report(struct sensrv *srv, tid_t client, struct sensmsg *msg)
{
    struct sensclient **cur;
    int now, rc;

    rc = Reply(client, NULL, 0);
    assertv(rc, rc == 0);

    now = Time(&srv->clock) - SENSOR_AVG_DELAY_TICKS;
    cur = &srv->clients_head;
    while (*cur != NULL) {
        struct sensmsg replymsg;
        unsigned i;
        bool reply;

        sensors_t *sensors = (*cur)->sensors;
        reply = false;
        for (i = 0; i < ARRAY_SIZE((*cur)->sensors); i++) {
            if (sensors[i] & msg->sensors[i]) {
                reply = true;
                replymsg.type = SENSOR_TRIPPED;
                replymsg.trigger_time = now;
                memcpy(
                    replymsg.sensors,
                    msg->sensors,
                    sizeof (replymsg.sensors));
                break;
            }
        }

        if (!reply) {
            int timeout = (*cur)->timeout;
            if (timeout >= 0 && timeout <= now) {
                reply = true;
                replymsg.type = SENSOR_TIMEOUT;
                replymsg.trigger_time = now;
            }
        }

        if (reply) {
            int rc = Reply((*cur)->tid, &replymsg, sizeof (replymsg));
            assertv(rc, rc == 0);
            (*cur)->tid = -1;
            *cur = (*cur)->next;
        } else {
            cur = &(*cur)->next;
        }
    }
}

void
sensorctx_init(struct sensorctx *ctx)
{
    ctx->sensrv_tid = WhoIs("sensrv");
}

int
sensor_wait(
    struct sensorctx *ctx,
    sensors_t sensors[SENSOR_MODULES],
    int timeout,
    int *when)
{
    struct sensmsg msg, reply;
    int rplylen;
    msg.type = SENSMSG_WAIT;
    memcpy(msg.sensors, sensors, sizeof (msg.sensors));
    msg.wait_timeout = timeout;
    rplylen = Send(ctx->sensrv_tid, &msg, sizeof (msg), &reply, sizeof (reply));
    assertv(rplylen, rplylen == sizeof (reply));
    memcpy(sensors, reply.sensors, sizeof (reply.sensors));
    if (when != NULL)
        *when = reply.trigger_time;
    return reply.type;
}

void
sensor_report(struct sensorctx *ctx, sensors_t sensors[SENSOR_MODULES])
{
    struct sensmsg msg;
    int rplylen;
    msg.type = SENSMSG_REPORT;
    memcpy(msg.sensors, sensors, sizeof (msg.sensors));
    rplylen = Send(ctx->sensrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}
