/*
 * bwio.c - busy-wait I/O routines for diagnosis
 *
 * Specific to the Beaglebone Black
 *
 */

#include "soc_AM335x.h"
#include "beaglebone.h"
#include "drv_uart.h"
#include "hw_types.h"

#include "xbool.h"
#include "xint.h"
#include "timer.h"
#include "xarg.h"
#include "gpio.h"
#include "bwio.h"

#define BWIO_UART (SOC_UART_0_REGS)
#define BAUD_RATE_115200 (115200)
#define UART_MODULE_INPUT_CLK (48000000)

#define GPIO_INSTANCE_ADDRESS (SOC_GPIO_1_REGS)
#define GPIO_INSTANCE_PIN_NUMBER (23)

void 
bwio_uart_setup(void)
{
    /* Set pin mux */
    UARTPinMuxSetup(0);

    /* Reset the UART */
    UARTModuleReset(BWIO_UART);

    /* Set the BAUD rate */
    unsigned int divisorValue = 0;

    /* Computing the Divisor Value. */
    divisorValue = UARTDivisorValCompute(UART_MODULE_INPUT_CLK,
                                         BAUD_RATE_115200,
                                         UART16x_OPER_MODE,
                                         UART_MIR_OVERSAMPLING_RATE_42);

    /* Programming the Divisor Latches. */
    UARTDivisorLatchWrite(BWIO_UART, divisorValue);

    /* Switching to Configuration Mode B. */
    UARTRegConfigModeEnable(BWIO_UART, UART_REG_CONFIG_MODE_B);

    /* Programming the Line Characteristics. */
    UARTLineCharacConfig(BWIO_UART,
                         (UART_FRAME_WORD_LENGTH_8 | UART_FRAME_NUM_STB_1),
                         UART_PARITY_NONE);

    /* Disabling write access to Divisor Latches. */
    UARTDivisorLatchDisable(BWIO_UART);

    /* Disabling Break Control. */
    UARTBreakCtl(BWIO_UART, UART_BREAK_COND_DISABLE);

    /* Switching to UART16x operating mode. */
    UARTOperatingModeSelect(BWIO_UART, UART16x_OPER_MODE);

}

int 
bwputc( char c ) {
    UARTCharPut(BWIO_UART, c);

    return 0;
}

char 
c2x( char ch ) {
    if ( (ch <= 9) ) return '0' + ch;
    return 'a' + ch - 10;
}

int 
bwputx( char c ) {
    char chh, chl;

    chh = c2x( c / 16 );
    chl = c2x( c % 16 );
    bwputc( chh );
    return bwputc( chl );
}

int 
bwputr( unsigned int reg ) {
    int byte;
    char *ch = (char *) &reg;

    for( byte = 3; byte >= 0; byte-- ) bwputx( ch[byte] );
    return bwputc( ' ' );
}

int 
bwputstr( char *str ) {
    while( *str ) {
	if( bwputc( *str ) < 0 ) return -1;
	str++;
    }
    return 0;
}

void 
bwputw( int n, char fc, char *bf ) {
    char ch;
    char *p = bf;

    while( *p++ && n > 0 ) n--;
    while( n-- > 0 ) bwputc( fc );
    while( ( ch = *bf++ ) ) bwputc( ch );
}

int 
bwgetc() {
    return (int)UARTCharGet(BWIO_UART);
}

int 
bwa2d( char ch ) {
    if( ch >= '0' && ch <= '9' ) return ch - '0';
    if( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
    if( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
    return -1;
}

char 
bwa2i( char ch, const char **src, int base, int *nump ) {
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

void 
bwui2a( unsigned int num, unsigned int base, char *bf ) {
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

void 
bwi2a( int num, char *bf ) {
    if( num < 0 ) {
	num = -num;
	*bf++ = '-';
    }
    bwui2a( num, 10, bf );
}

void 
bwformat ( const char *fmt, va_list va ) {
    char bf[12];
    char ch, lz;
    int w;

	
    while ( ( ch = *(fmt++) ) ) {
	if ( ch != '%' )
	    bwputc( ch );
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
		bwputc( va_arg( va, char ) );
		break;
	    case 's':
		bwputw( w, 0, va_arg( va, char* ) );
		break;
	    case 'u':
		bwui2a( va_arg( va, unsigned int ), 10, bf );
		bwputw( w, lz, bf );
		break;
	    case 'd':
		bwi2a( va_arg( va, int ), bf );
		bwputw( w, lz, bf );
		break;
	    case 'x':
	    case 'p':
		bwui2a( va_arg( va, unsigned int ), 16, bf );
		bwputw( w, lz, bf );
		break;
	    case '%':
		bwputc( ch );
		break;
	    }
	}
    }
}

void 
bwprintf( const char *fmt, ... ) {
    va_list va;

    va_start(va,fmt);
    bwformat( fmt, va );
    va_end(va);
}

