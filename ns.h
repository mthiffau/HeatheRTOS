#ifdef NS_H
#error "double-included ns.h"
#endif

#define NS_H

#define NS_NAME_MAXLEN 32
#define NS_TID          1

enum {
    NS_MSG_REGISTER,
    NS_MSG_WHOIS
};

struct ns_msg {
    int  type;
    char name[NS_NAME_MAXLEN]; /* NUL-terminated */
};

/* Replies to a registration message. (WhoIs always succeeds) */
enum {
    NS_RPLY_SUCCESS = 0,
    NS_RPLY_NOSPACE = -3
};

/* Name server entry point.
 * The name server should be started at the beginning of u_init */
void ns_main(void);
