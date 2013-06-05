#include "config.h"
#include "xdef.h"
#include "u_tid.h"
#include "u_syscall.h"

#include "xint.h"
#include "xbool.h"
#include "xassert.h"
#include "xarg.h"
#include "bwio.h"

#include "u_rps_common.h"
#include "u_rpsc.h"

static int get_random_int(void);
static void set_seed(int seed);

void
u_rpsc_main(void)
{
    int rc, i;
    tid_t rpss_tid, me;
    struct rps_msg msg;
    struct rps_msg rply;

    /* Init random generator */
    set_seed(3);

    /* Whoami? */
    me = MyTid();

    /* Find out who the RPS server is */
    rpss_tid = WhoIs(RPSS_NAME);

    bwprintf(COM2,
             "TID=%d found the game server at %d, signing up...\n",
             me,
             rpss_tid
        );

    /* Sign Up */
    msg.type = RPS_MSG_SIGNUP;
    rc = Send(rpss_tid, &msg, sizeof(msg), &rply, sizeof(rply));
    assert(rc == sizeof(rply));
    assert(rply.type == RPS_MSG_ACK);

    bwprintf(COM2,
             "TID=%d Signed up and paired with a partner, sending moves...\n", me);

    /* Play randomly some number of times */
    for(i = 0; i < TIMES_TO_PLAY; i++) {
        msg.type = RPS_MSG_PLAY;

        msg.move = 1 + (get_random_int() % 3);
        rc = Send(rpss_tid, &msg, sizeof(msg), &rply, sizeof(rply));
        assert(rc == sizeof(rply));
        if (rply.type == RPS_MSG_NACK) {
            bwprintf(COM2, "TID=%d Partner quit, exiting...\n", me);
            Exit();
        }
    }

    bwprintf(COM2, "TID=%d Played all my games, quitting...\n", me);

    /* Quit */
    msg.type = RPS_MSG_QUIT;
    rc = Send(rpss_tid, &msg, sizeof(msg), &rply, sizeof(rply));
    assert(rc == sizeof(msg));
    assert(rply.type == RPS_MSG_ACK);

    bwprintf(COM2, "TID=%d Successfully quit, exiting...\n", me);
}

/* Return a random value */
static int random_seed;

static void
set_seed(int seed)
{
    random_seed = seed;
}

static int
get_random_int()
{
    int m  = 65000;
    int a  = 261;
    int c  = 9 * 7 * 11;
    int r;

    r = ((a * random_seed) + c) % m;
    random_seed = r;

    return r;
}

