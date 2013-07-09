#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "u_tid.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "tracksel_srv.h"

#include "xassert.h"
#include "queue.h"

#include "u_syscall.h"
#include "ns.h"

enum {
    TSMSG_TELL,
    TSMSG_ASK,
};

struct tsmsg {
    int           type;
    track_graph_t track; /* TSMSG_TELL only */
};

void
trackselsrv_main(void)
{
    track_graph_t track;
    struct queue  asks;
    tid_t         asks_mem[MAX_TASKS];

    struct tsmsg msg;
    tid_t client;
    int rc, msglen;

    /* Initialize */
    track = NULL;
    q_init(&asks, asks_mem, sizeof (asks_mem[0]), sizeof (asks_mem));

    /* Register */
    rc = RegisterAs("tracksel");
    assertv(rc, rc == 0);

    /* Event loop */
    for (;;) {
        /* Get message */
        msglen = Receive(&client, &msg, sizeof (msg));
        assertv(msglen, msglen == sizeof (msg));
        switch (msg.type) {
        case TSMSG_ASK:
            if (track != NULL) {
                rc = Reply(client, &track, sizeof (track));
                assertv(rc, rc == 0);
            } else {
                q_enqueue(&asks, &client);
            }
            break;
        case TSMSG_TELL:
            rc = Reply(client, NULL, 0);
            assertv(rc, rc == 0);
            assert(track == NULL);
            assert(msg.track != NULL);
            track = msg.track;
            while (q_dequeue(&asks, &client)) {
                rc = Reply(client, &track, sizeof (track));
                assertv(rc, rc == 0);
            }
            break;
        default:
            panic("invalid track selection server message type %d", msg.type);
        }
    }
}

track_graph_t
tracksel_ask(void)
{
    track_graph_t g;
    struct tsmsg msg;
    int rplylen;
    msg.type = TSMSG_ASK;
    rplylen  = Send(WhoIs("tracksel"), &msg, sizeof (msg), &g, sizeof (g));
    assertv(rplylen, rplylen == sizeof (g));
    return g;
}

void
tracksel_tell(track_graph_t track)
{
    struct tsmsg msg;
    int rplylen;
    msg.type  = TSMSG_TELL;
    msg.track = track;
    rplylen = Send(WhoIs("tracksel"), &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}
