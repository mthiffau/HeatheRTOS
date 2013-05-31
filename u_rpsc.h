#ifdef U_RPSC_H
#error "double-included u_rpsc.h"
#endif

#define U_RPSC_H

U_RPS_COMMON_H;

#define TIMES_TO_PLAY (5) /* Number of games a client will play */

/* Rock Paper Scissors Client Entry Point */
void u_rpsc_main(void);
