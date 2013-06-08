#ifdef NS_H
#error "double-included ns.h"
#endif

#define NS_H

U_TID_H;

#define NS_NAME_MAXLEN 32
#define NS_TID          1

/* Name server entry point.
 * The name server should be started at the beginning of u_init */
void ns_main(void);

/* Register with the name server under the given name.
 *
 * The name must fit into NS_NAME_MAXLEN characters,
 * including the trailing NUL. */
int RegisterAs(const char *name);

/* Find the TID of the task registered with the given name.
 *
 * If there is no task registered with that name, then WhoIs blocks
 * until such a task becomes registered.
 *
 * The name must fit into NS_NAME_MAXLEN characters,
 * including the trailing NUL. */
tid_t WhoIs(const char *name);
