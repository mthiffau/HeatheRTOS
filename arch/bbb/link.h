#ifndef LINK_H
#define LINK_H

/* Declarations for memory locations defined by the linker script */
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

#endif
