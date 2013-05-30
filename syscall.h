#ifdef SYSCALL_H
#error "double-included syscall.h"
#endif

#define SYSCALL_H

#define SYSCALL_CREATE          0x11
#define SYSCALL_MYTID           0x12
#define SYSCALL_MYPARENTTID     0x13
#define SYSCALL_PASS            0x14
#define SYSCALL_EXIT            0x15
#define SYSCALL_DESTROY         0x16
#define SYSCALL_SEND            0x21
#define SYSCALL_RECEIVE         0x22
#define SYSCALL_REPLY           0x23
#define SYSCALL_AWAITEVENT      0x41
#define SYSCALL_SHUTDOWN        0x51
