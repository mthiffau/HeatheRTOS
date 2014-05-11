#ifdef U_INIT_H
#error "double-included u_init.h"
#endif

#define U_INIT_H

enum { U_INIT_PRIORITY = 1 };

/* Entry point for init */
void u_init_main(void);
