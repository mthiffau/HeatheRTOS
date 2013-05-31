#include "config.h"
#include "u_tid.h"
#include "u_syscall.h"

#include "xint.h"
#include "xdef.h"
#include "xbool.h"
#include "xassert.h"
#include "xarg.h"
#include "bwio.h"

#include "u_rps_common.h"
#include "u_rpss.h"

static void u_rpss_init(struct rpss_state* state);

static void u_rpss_cli_enqueue(struct rpss_state* state, struct rps_client* cli);
static struct rps_client* u_rpss_cli_dequeue(struct rpss_state* state);

static void u_rpss_match_free(struct rpss_state* state, struct rps_match* match);
static struct rps_match* u_rpss_match_alloc(struct rpss_state* state);

static void u_rpss_handle_signup(struct rpss_state* state, int cli_tid);
static void u_rpss_handle_play(struct rpss_state* state, int cli_tid, uint8_t move);
static void u_rpss_handle_quit(struct rpss_state* state, int cli_tid);

static const char* move_names[] = {
    "None",
    "Rock",
    "Paper",
    "Scissors"
};

/* Rock Paper Scissors Server Entry Point */
void
u_rpss_main(void)
{
    int msglen, sender, rc;
    struct rps_msg msg;
    struct rpss_state srv_state;

    /* Initialize the server data */
    u_rpss_init(&srv_state);

    /* Register with the nameserver */
    rc = RegisterAs(RPSS_NAME);

    for(;;) {
        /* Receive a request */
        msglen = Receive(&sender, &msg, sizeof(msg));
        assert(msglen == sizeof(msg));

        /* Decide what to do */ 
        switch(msg.type) {
        case RPS_MSG_SIGNUP:
            u_rpss_handle_signup(&srv_state, sender);
            break;
        case RPS_MSG_PLAY:
            u_rpss_handle_play(&srv_state, sender, msg.move);
            break;
        case RPS_MSG_QUIT:
            u_rpss_handle_quit(&srv_state, sender);
            break;
        default:
            assert(false);
        }
    }
}

static void 
u_rpss_init(struct rpss_state* state)
{
    int i;
    
    /* Init the queue */
    state->client_queue.count = 0;
    state->client_queue.head = RPS_CLIENT_NONE;
    state->client_queue.tail = RPS_CLIENT_NONE;

    /* Add all matches to free list */
    for(i = 0; i < MAX_MATCHES; i++) {
        state->matches[i].player1 = RPS_CLIENT_NONE;
        state->matches[i].player2 = RPS_CLIENT_NONE;
        state->matches[i].next = i + 1;
    }
    state->matches[MAX_MATCHES-1].next = RPS_MATCH_NONE;
    state->free_matches.count = MAX_MATCHES;
    state->free_matches.head = 0;
    state->free_matches.tail = MAX_MATCHES - 1;

    /* Init the client structures */
    for(i = 0; i < MAX_CLIENTS; i++) {
        state->clients[i].tid = RPS_CLIENT_NONE;
        state->clients[i].move = RPS_MOVE_NONE;
        state->clients[i].match = RPS_MATCH_NONE;
        state->clients[i].next = RPS_CLIENT_NONE;
    }
}

static void 
u_rpss_handle_signup(struct rpss_state* state, int cli_tid)
{
    int rc;
    uint8_t cli_ix = CLI_TID2IX(cli_tid);
    struct rps_msg msg;
    struct rps_client* new_cli;
    struct rps_client* waiting_cli;
    struct rps_match* match;

    bwprintf(COM2, "SERVER: Handling signup from %d\n", cli_tid);

    /* Check if we've already had a signup from this client */
    if(state->clients[cli_ix].tid != RPS_CLIENT_NONE) {
        msg.type = RPS_MSG_NACK;
        rc = Reply(cli_tid, &msg, sizeof(msg));
        assert(rc == 0);
        return;
    }

    new_cli = &(state->clients[cli_ix]);

    /* Dequeue our opponent.
       If no opponent, enqueue ourselves. */
    waiting_cli = u_rpss_cli_dequeue(state);
    if (waiting_cli == NULL) {
        new_cli->tid = cli_tid;
        u_rpss_cli_enqueue(state, new_cli);
        return;
    }
    
    /* Try and allocate a match.
       If we fail, send both clients a nack */
    match = u_rpss_match_alloc(state);
    if (match == NULL) {
        msg.type = RPS_MSG_NACK;

        rc = Reply(cli_tid, &msg, sizeof(msg));
        assert(rc == 0);
        rc = Reply(waiting_cli->tid, &msg, sizeof(msg));
        assert(rc == 0);
        waiting_cli->tid = RPS_CLIENT_NONE;
        return;
    }
    
    new_cli->tid = cli_tid;

    /* Set up the match */
    match->player1 = CLI_PTR2IX(state, waiting_cli);
    match->player2 = cli_ix;
    waiting_cli->match = MATCH_PTR2IX(state, match);
    new_cli->match = MATCH_PTR2IX(state, match);

    /* Tell both clients they can play now */
    msg.type = RPS_MSG_ACK;
    rc = Reply(waiting_cli->tid, &msg, sizeof(msg));
    assert(rc == 0);
    rc = Reply(cli_tid, &msg, sizeof(msg));
    assert(rc == 0);
}

static void 
u_rpss_handle_play(struct rpss_state* state, int cli_tid, uint8_t move)
{
    int rc;
    char c;
    uint8_t cli_ix = CLI_TID2IX(cli_tid);
    struct rps_msg msg;
    struct rps_client* player1;
    struct rps_client* player2;
    struct rps_client* winner;
    struct rps_match* match;
    
    player1 = &(state->clients[cli_ix]);
    /* Check if it's a valid client */
    if(player1->tid != cli_tid) {
        msg.type = RPS_MSG_NACK;
        rc = Reply(cli_tid, &msg, sizeof(msg));
        assert(rc == 0);
        return;
    }

    /* Set our move */
    assert(player1->move == RPS_MOVE_NONE);
    player1->move = move;

    /* Get the match and fix our p1/p2 pointers */
    match = &(state->matches[player1->match]);
    player1 = &(state->clients[match->player1]);
    player2 = &(state->clients[match->player2]);
    
    /* If both clients made their move, 
       resolve the match */
    if (player1->move != RPS_MOVE_NONE &&
        player2->move != RPS_MOVE_NONE) {
        
        /* Was it a draw? */
        if (player1->move == player2->move) {
            bwprintf(COM2,
                     "Player 1 (TID: %d) and 2 (TID: %d) both played %s, it was a draw\n\n",
                     player1->tid,
                     player2->tid,
                     move_names[player1->move]
                );
        } else {
            switch(player1->move) {
            case RPS_MOVE_ROCK:
                winner = player2->move == RPS_MOVE_SCISSORS ? player1 : player2;
                break;
            case RPS_MOVE_PAPER:
                winner = player2->move == RPS_MOVE_ROCK ? player1 : player2;
                break;
            case RPS_MOVE_SCISSORS:
                winner = player2->move == RPS_MOVE_PAPER ? player1 : player2;
                break;
            default:
                assert(false);
            }
            
            bwprintf(COM2, 
                     "Player 1 (TID: %d) played %s\n"
                     "Player 2 (TID: %d) played %s\n"
                     "Player %d (TID: %d) was victorious\n\n",
                     player1->tid,
                     move_names[player1->move],
                     player2->tid,
                     move_names[player2->move],
                     (winner == player1 ? 1 : 2),
                     winner->tid
                );
        }
        
        player1->move = RPS_MOVE_NONE;
        player2->move = RPS_MOVE_NONE;
        c = bwgetc(COM2); /* Wait to continue */

        /* Unblock players to play again */
        msg.type = RPS_MSG_ACK;
        rc = Reply(player1->tid, &msg, sizeof(msg));
        assert(rc == 0);
        rc = Reply(player2->tid, &msg, sizeof(msg));
        assert(rc == 0);
    }
}

static void 
u_rpss_handle_quit(struct rpss_state* state, int cli_tid)
{
    int rc;
    uint8_t cli_ix = CLI_TID2IX(cli_tid);
    struct rps_msg msg;
    struct rps_client* player1;
    struct rps_client* player2;
    struct rps_match* match;
    
    player1 = &(state->clients[cli_ix]);
    /* Check if it's a valid client */
    if(player1->tid != cli_tid) {
        msg.type = RPS_MSG_ACK;
        rc = Reply(cli_tid, &msg, sizeof(msg));
        assert(rc == 0);
        return;
    }
    
    /* If the tid is valid but there's no assigned
       match, that means that this client is still
       in the queue waiting for an opponent */
    if (player1->match == RPS_MATCH_NONE) {
        player2 = u_rpss_cli_dequeue(state);
        assert(player1 == player2);
        player1->tid = RPS_CLIENT_NONE;
    } else {
        /* Otherwise free up both client handles and the match.
           If the other guy made a move, send him nack, 
           otherwise don't worry about it, he'll get one when
           he plays */
        match = &(state->matches[player1->match]);
        if (cli_ix == match->player1) {
            player2 = &(state->clients[match->player2]);
        } else {
            player2 = &(state->clients[match->player1]);
        }
        /* Need to send NACK if the other guy has played */
        if (player2->move != RPS_MOVE_NONE) {
            msg.type = RPS_MSG_NACK;
            rc = Reply(cli_tid, &msg, sizeof(msg));
            assert(rc == 0);
        }
        player1->tid = RPS_CLIENT_NONE;
        player2->tid = RPS_CLIENT_NONE;
        player1->match = RPS_MATCH_NONE;
        player2->match = RPS_MATCH_NONE;
        u_rpss_match_free(state, match);
    }

    msg.type = RPS_MSG_ACK;
    rc = Reply(cli_tid, &msg, sizeof(msg));
    assert(rc == 0);
}

/* Enqueue a client */
static void 
u_rpss_cli_enqueue(struct rpss_state* state, struct rps_client* cli)
{
    int old_tail;

    assert(cli->next == RPS_CLIENT_NONE);

    old_tail = state->client_queue.tail;

    if (old_tail == RPS_CLIENT_NONE) {
        assert(state->client_queue.head == RPS_CLIENT_NONE);
        assert(state->client_queue.count == 0);

        state->client_queue.head = CLI_PTR2IX(state, cli);
    } else {
        state->clients[old_tail].next = CLI_PTR2IX(state, cli);
    }

    state->client_queue.tail = CLI_PTR2IX(state, cli);
    state->client_queue.count++;
}

/* Dequeue a client */
static struct rps_client* 
u_rpss_cli_dequeue(struct rpss_state* state)
{
    int old_head, new_head;

    if (state->client_queue.count == 0)
        return NULL;

    old_head = state->client_queue.head;
    new_head = state->clients[old_head].next;
    if (new_head == RPS_CLIENT_NONE) {
        assert(state->client_queue.count == 1);
        state->client_queue.tail = RPS_CLIENT_NONE;
    } 
    
    state->client_queue.head = new_head;
    state->client_queue.count--;

    return &(state->clients[old_head]);
}

/* Free a match descriptor */
static void 
u_rpss_match_free(struct rpss_state* state, struct rps_match* match) 
{
    match->player1 = RPS_CLIENT_NONE;
    match->player2 = RPS_CLIENT_NONE;

    match->next = state->free_matches.head;
    state->free_matches.head = MATCH_PTR2IX(state, match);
    if (match->next == RPS_MATCH_NONE) {
        assert(state->free_matches.count == 0);
        state->free_matches.tail = state->free_matches.head;
    }

    state->free_matches.count++;
}

/* Grab an unused match descriptor */
static struct rps_match* 
u_rpss_match_alloc(struct rpss_state* state) 
{
    int old_head, new_head;

    if (state->free_matches.count == 0)
        return NULL;

    old_head = state->free_matches.head;
    new_head = state->matches[old_head].next;
    state->free_matches.head = new_head;
    if (new_head == RPS_MATCH_NONE) {
        assert(state->free_matches.count == 1);
        state->free_matches.tail = RPS_MATCH_NONE;
    }

    state->free_matches.count--;

    return &(state->matches[old_head]);
}
