#ifdef LINK_H
#error "double-included link.h"
#endif

#define LINK_H

#define KernStackTop ((void*)(&_KernStackTop))
extern char _KernStackTop;

#define KernStackBottom ((void*)(&_KernStackBottom))
extern char _KernStackBottom;

#define UserStacksStart ((void*)(&_UserStacksStart))
extern char _UserStacksStart;

#define UserStacksEnd ((void*)(&_UserStacksEnd))
extern char _UserStacksEnd;

#define UndefInstrStackTop ((void*)(&_UndefInstrStackTop))
extern char _UndefInstrStackTop;

#define UndefInstrStackBottom ((void*)(&_UndefInstrStackBottom))
extern char _UndefInstrStackBottom;
