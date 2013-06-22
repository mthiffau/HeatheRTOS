#ifdef U_EVENTS_H
#error "double-included u_events.h"
#endif

#define U_EVENTS_H

enum {
    EV_CLOCK_TICK = 0,
    EV_UART1_RX   = 1,
    EV_UART1_TX   = 2,
    EV_UART1_GEN  = 3,
    EV_UART2_RX   = 4,
    EV_UART2_TX   = 5,
    EV_UART2_GEN  = 6
};

enum {
    IRQ_CLOCK_TICK = 51,
    IRQ_UART1_RX   = 23,
    IRQ_UART1_TX   = 24,
    IRQ_UART1_GEN  = 52,
    IRQ_UART2_RX   = 25,
    IRQ_UART2_TX   = 26,
    IRQ_UART2_GEN  = 54
};
