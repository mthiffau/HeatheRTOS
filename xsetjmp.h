#ifdef XSETJMP_H
#error "double-included xsetjmp.h"
#endif

#define XSETJMP_H

typedef struct __jmp_buf {
    unsigned int regs[10];
} jmp_buf[1];

int  setjmp(jmp_buf buf);
void longjmp(jmp_buf buf, int val)
    __attribute__((noreturn));
