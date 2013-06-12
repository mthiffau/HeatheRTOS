#undef NOASSERT

#include "u_tid.h"

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "xarg.h"
#include "bwio.h"
#include "pqueue.h"
#include "timer.h"

#include "xassert.h"
#include "u_syscall.h"
#include "u_events.h"
#include "ns.h"
#include "array_size.h"

#define EVLOG_BUFSIZE   128

#define TEST_NODES MAX_TASKS

static struct pqueue test_q;
static struct pqueue_node test_nodes[TEST_NODES];

void
test_queue_impl(void)
{
    int i, rc, nodes, insrt_time, pop_time;

    insrt_time = 0;
    pop_time = 0;
    rc = 0;

    bwprintf(COM2, "\ntest_queue_impl...\n\n");

    pqueue_init(&test_q, ARRAY_SIZE(test_nodes), test_nodes);

    for (nodes = 4; nodes <= TEST_NODES; nodes *= 2) {

        /* Insertion speed test */
        tmr40_reset();
        for(i = 0; i < nodes; i++) {
            rc += pqueue_add(&test_q, i, i);
        }
        insrt_time = tmr40_get();
        assertv(rc, rc == 0);

        /* Pop speed test */
        tmr40_reset();
        for(i = 0; i < nodes; i++) {
            pqueue_popmin(&test_q);
        }
        pop_time = tmr40_get();

        bwprintf(COM2, 
                 "%d nodes\nInsert: %d\nPop: %d\n\n",
                 nodes,
                 insrt_time,
                 pop_time
            );
    }

}
