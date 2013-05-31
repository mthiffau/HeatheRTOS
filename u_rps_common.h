#ifdef U_RPS_COMMON_H
#error "double-included u_rps_common.h"
#endif

#define U_RPS_COMMON_H

XINT_H;

#define RPSS_NAME "RPSS"

/* RPS moves */
enum {
    RPS_MOVE_NONE,
    RPS_MOVE_ROCK,
    RPS_MOVE_PAPER,
    RPS_MOVE_SCISSORS
};

/* RPS message types */
enum {
    RPS_MSG_ACK,
    RPS_MSG_NACK,
    RPS_MSG_SIGNUP,
    RPS_MSG_PLAY,
    RPS_MSG_QUIT,
};

/* RPS message format */
struct rps_msg {
    uint8_t type;
    uint8_t move;
};
