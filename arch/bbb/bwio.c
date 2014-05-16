/*******************************************************************************
    Copyright 2014 Matthew Thiffault

    This file is part of HeatheRTOS.

    HeatheRTOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    HeatheRTOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HeatheRTOS.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************/
/*
* Some of this code is pulled from TI's BeagleBone Black StarterWare
* Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
*
*  Redistribution and use in source and binary forms, with or without 
*  modification, are permitted provided that the following conditions 
*  are met:
*
*    Redistributions of source code must retain the above copyright 
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the 
*    documentation and/or other materials provided with the   
*    distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

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

/* Configure polling IO on default UART */
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

/* Polling/blocking write a character to the default UART */
int 
bwputc( char c ) {
    UARTCharPut(BWIO_UART, c);

    return 0;
}

/* Hex print helper functions */
char 
c2x( char ch ) {
    if ( (ch <= 9) ) return '0' + ch;
    return 'a' + ch - 10;
}

/* Print a byte in hex */
int 
bwputx( char c ) {
    char chh, chl;

    chh = c2x( c / 16 );
    chl = c2x( c % 16 );
    bwputc( chh );
    return bwputc( chl );
}

/* Print the contents of a register in hex */
int 
bwputr( unsigned int reg ) {
    int byte;
    char *ch = (char *) &reg;

    for( byte = 3; byte >= 0; byte-- ) bwputx( ch[byte] );
    return bwputc( ' ' );
}

/* Print a null terminated string */
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

/* Polling/blocking get character from default UART */
int 
bwgetc() {
    return (int)UARTCharGet(BWIO_UART);
}

/* Formatting/conversion helper functions */
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

/* Format function for bwprintf */
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

/* Polling IO printf */
void 
bwprintf( const char *fmt, ... ) {
    va_list va;

    va_start(va,fmt);
    bwformat( fmt, va );
    va_end(va);
}

