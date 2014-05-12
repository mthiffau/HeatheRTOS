#ifndef BWIO_H
#define BWIO_H

#include "xarg.h"

/* Configure polling IO on default UART */
void bwio_uart_setup(void);

/* Polling/blocking write a character to the default UART */
int bwputc( char c );

/* Polling/blocking get character from default UART */
int bwgetc();

/* Print a number in hex */
int bwputx( char c );

/* Print a null terminated string */
int bwputstr( char *str );

/* Print the contents of a register in hex */
int bwputr( unsigned int reg );

void bwputw( int n, char fc, char *bf );

/* Format function for printf */
void bwformat( const char *format, va_list args );

/* Polling IO printf */
void bwprintf( const char *format, ... )
    __attribute__((format(printf, 1, 2)));

/* Formatting helper functions */
void bwui2a(unsigned int num, unsigned int base, char *bf);
void bwi2a(int num, char *bf);

#endif
