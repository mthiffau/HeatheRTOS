#ifdef LINK_H
#error "double-included link.h"
#endif

#define LINK_H

#define KernStackTop ((void*)(&_KernStackTop))
extern char _KernStackTop;

#define KernStackBottom ((void*)0x4030FFF0)

#define UserStacksStart ((void*)(&_UserStacksStart))
extern char _UserStacksStart;

#define UserStacksEnd ((void*)0x9FFFFFF0)
