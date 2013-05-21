#ifdef LINK_H
#error "double-included link.h"
#endif

#define LINK_H

#define ProgramEnd  ((void*)(&_ProgramEnd))
extern char _ProgramEnd;
