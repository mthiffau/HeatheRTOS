#ifdef U_RPSS_H
#error "double-included u_rpss.h"
#endif

#define U_RPSS_H

XINT_H;
U_TID_H;
U_RPS_COMMON_H;
CONFIG_H;

#define MAX_CLIENTS MAX_TASKS
#define MAX_MATCHES (MAX_TASKS/2)
#define RPS_CLIENT_NONE ((uint8_t)(-1))
#define RPS_MATCH_NONE ((uint8_t)(-1))

#define CLI_TID2IX(tid) \
    ((uint8_t) ((tid) & 0xff))
#define CLI_PTR2IX(state, ptr) \
    ((uint8_t) (ptr - (state)->clients))
#define MATCH_PTR2IX(state, ptr) \
    ((uint8_t) (ptr - (state)->matches))

/* Stores info about a registered RPS client */
struct rps_client {
    uint8_t next;    /* Next player in a queue */
    tid_t tid;       /* TID of player */
    uint8_t move;    /* Player's currently selected move, R, P, S or none */
    uint8_t match;   /* Index of the match the player is participating in */
};

/* Represent an ongoing match between two clients */
struct rps_match {
    uint8_t next;
    uint8_t player1;
    uint8_t player2;
};

/* Queue of clients waiting to play */
struct rps_queue {
    uint8_t count;
    uint8_t head;
    uint8_t tail;
};

/* State of the RPS server */
struct rpss_state {
    struct rps_client clients[MAX_CLIENTS];
    struct rps_match  matches[MAX_MATCHES];
    struct rps_queue  free_matches;
    struct rps_queue  client_queue;
};

/* Rock Paper Scissors Server Entry Point */
void u_rpss_main(void);
