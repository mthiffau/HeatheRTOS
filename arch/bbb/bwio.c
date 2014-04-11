/*
 * bwio.c - busy-wait I/O routines for diagnosis
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */

#include "xarg.h"
#include "bwio.h"

/*
 * The UARTs are initialized by RedBoot to the following state
 * 	115,200 bps
 * 	8 bits
 * 	no parity
 * 	fifos enabled
 */
int bwsetfifo( int channel, int state ) {
    (void)channel;
    (void)state;
  
    return 0;
}

int bwsetspeed( int channel, int speed ) {
    (void)channel;
    (void)speed;

    return 0;
}

int bwputc( int channel, char c ) {
    (void)channel;
    (void)c;
  
    return 0;
}

char c2x( char ch ) {
    if ( (ch <= 9) ) return '0' + ch;
    return 'a' + ch - 10;
}

int bwputx( int channel, char c ) {
    char chh, chl;

    chh = c2x( c / 16 );
    chl = c2x( c % 16 );
    bwputc( channel, chh );
    return bwputc( channel, chl );
}

int bwputr( int channel, unsigned int reg ) {
    int byte;
    char *ch = (char *) &reg;

    for( byte = 3; byte >= 0; byte-- ) bwputx( channel, ch[byte] );
    return bwputc( channel, ' ' );
}

int bwputstr( int channel, char *str ) {
    while( *str ) {
	if( bwputc( channel, *str ) < 0 ) return -1;
	str++;
    }
    return 0;
}

void bwputw( int channel, int n, char fc, char *bf ) {
    char ch;
    char *p = bf;

    while( *p++ && n > 0 ) n--;
    while( n-- > 0 ) bwputc( channel, fc );
    while( ( ch = *bf++ ) ) bwputc( channel, ch );
}

int bwgetc( int channel ) {
    (void)channel;
  
    return 0;
}

int bwa2d( char ch ) {
    if( ch >= '0' && ch <= '9' ) return ch - '0';
    if( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
    if( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
    return -1;
}

char bwa2i( char ch, const char **src, int base, int *nump ) {
    int num, digit;
    const char *p;

    p = *src; num = 0;
    while( ( digit = bwa2d( ch ) ) >= 0 ) {
	if ( digit > base ) break;
	num = num*base + digit;
	ch = *p++;
    }
    *src = p; *nump = num;
    return ch;
}

void bwui2a( unsigned int num, unsigned int base, char *bf ) {
    int n = 0;
    int dgt;
    unsigned int d = 1;
	
    while( (num / d) >= base ) d *= base;
    while( d != 0 ) {
	dgt = num / d;
	num %= d;
	d /= base;
	if( n || dgt > 0 || d == 0 ) {
	    *bf++ = dgt + ( dgt < 10 ? '0' : 'a' - 10 );
	    ++n;
	}
    }
    *bf = 0;
}

void bwi2a( int num, char *bf ) {
    if( num < 0 ) {
	num = -num;
	*bf++ = '-';
    }
    bwui2a( num, 10, bf );
}

void bwformat ( int channel, const char *fmt, va_list va ) {
    char bf[12];
    char ch, lz;
    int w;

	
    while ( ( ch = *(fmt++) ) ) {
	if ( ch != '%' )
	    bwputc( channel, ch );
	else {
	    lz = 0; w = 0;
	    ch = *(fmt++);
	    switch ( ch ) {
	    case '0':
		lz = 1; ch = *(fmt++);
		break;
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
		ch = bwa2i( ch, &fmt, 10, &w );
		break;
	    }
	    switch( ch ) {
	    case 0: return;
	    case 'c':
		bwputc( channel, va_arg( va, char ) );
		break;
	    case 's':
		bwputw( channel, w, 0, va_arg( va, char* ) );
		break;
	    case 'u':
		bwui2a( va_arg( va, unsigned int ), 10, bf );
		bwputw( channel, w, lz, bf );
		break;
	    case 'd':
		bwi2a( va_arg( va, int ), bf );
		bwputw( channel, w, lz, bf );
		break;
	    case 'x':
	    case 'p':
		bwui2a( va_arg( va, unsigned int ), 16, bf );
		bwputw( channel, w, lz, bf );
		break;
	    case '%':
		bwputc( channel, ch );
		break;
	    }
	}
    }
}

void bwprintf( int channel, const char *fmt, ... ) {
    va_list va;

    va_start(va,fmt);
    bwformat( channel, fmt, va );
    va_end(va);
}

