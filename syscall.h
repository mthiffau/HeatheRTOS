#ifdef SYSCALL_H
#error "double-included syscall.h"
#endif

#define SYSCALL_H

#define SYSCALL_CREATE          0x0
#define SYSCALL_MYTID           0x1
#define SYSCALL_MYPARENTTID     0x2
#define SYSCALL_PASS            0x3
#define SYSCALL_EXIT            0x4
#define SYSCALL_DESTROY         0x5
#define SYSCALL_SEND            0x6
#define SYSCALL_RECEIVE         0x7
#define SYSCALL_REPLY           0x8
#define SYSCALL_REGISTERCLEANUP 0x9
#define SYSCALL_REGISTEREVENT   0xa
#define SYSCALL_AWAITEVENT      0xb
#define SYSCALL_SHUTDOWN        0xc
