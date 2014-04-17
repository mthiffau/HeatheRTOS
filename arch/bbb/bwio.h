/*
 * bwio.h
 */

#ifdef BWIO_H
#error "double-included bwio.h"
#endif

#define BWIO_H

XARG_H;

#define COM1	0
#define COM2	1

#define ON	1
#define	OFF	0

void bwio_uart_setup(void);

int bwputc( char c );

int bwgetc();

int bwputx( char c );

int bwputstr( char *str );

int bwputr( unsigned int reg );

void bwputw( int n, char fc, char *bf );

void bwformat( const char *format, va_list args );

void bwprintf( const char *format, ... )
    __attribute__((format(printf, 1, 2)));
