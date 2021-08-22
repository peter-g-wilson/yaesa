#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "pico/binary_info.h"

#include "proj_board.h"
#include "serial_io.h"

#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

typedef struct serialRxCallBack_struct {
    uint8_t               serioRxId;
    uartIO_rxCallBack_t * serioRxFn;
    uint8_t *             serioRxBuf;
    size_t                serioRxMax;
    volatile bool         serioRxEnbl;
    repeating_timer_t     serioRxPollTmr;
} serialRxCallBack_t;

serialRxCallBack_t uartioRx;
serialRxCallBack_t stdioioRx;

bool serioRx_tmrCallBack(struct repeating_timer *t) {
    static size_t ipIdx = 0;
    if (stdioioRx.serioRxEnbl) {
        while (true) {
            int ch = getchar_timeout_us(0);
            if (ch < 0) break;
            if (ch > 127) continue;
            stdioioRx.serioRxBuf[ipIdx++] = ch;
            if ((ch == 10) || (ipIdx >= stdioioRx.serioRxMax)) {
                int (*fp) (uint8_t, uint8_t*, int) = (void *) stdioioRx.serioRxFn;
                int rdyNxt = fp( stdioioRx.serioRxId, stdioioRx.serioRxBuf, ipIdx );
                ipIdx = 0;
                if (!rdyNxt) {
                    stdioioRx.serioRxEnbl = false;
                    break;
                }                    
            }
        }
    }
    return true;
}
void stdioRx_init( uint8_t id, uartIO_rxCallBack_t *rxCallBack, uint8_t *ipBuf, size_t ipMaxLen ) {
    stdioioRx.serioRxId   = id;
    stdioioRx.serioRxFn   = rxCallBack;
    stdioioRx.serioRxBuf  = ipBuf;
    stdioioRx.serioRxMax  = ipMaxLen;
    stdioioRx.serioRxEnbl = false;

    add_repeating_timer_ms( 2, serioRx_tmrCallBack, (void *) 0,  &stdioioRx.serioRxPollTmr );
}
void stdioRx_enable(bool isEnabled) {
    stdioioRx.serioRxEnbl = isEnabled;
}

void uartIO_rxCallBack() {
    static size_t ipIdx = 0;
    while (uartioRx.serioRxEnbl && uart_is_readable(UART_HW)) {
        if (ipIdx < uartioRx.serioRxMax) {
            uint8_t ch = uart_getc(UART_HW);
            if (ch < 128) {
                uartioRx.serioRxBuf[ipIdx++] = ch;
                if ((ch == 10) || (ipIdx >= uartioRx.serioRxMax)) {
                    int (*fp) (uint8_t, uint8_t*, int) = (void *) uartioRx.serioRxFn;
                    int rdyNxt = fp( uartioRx.serioRxId, uartioRx.serioRxBuf, ipIdx );
                    if (!rdyNxt) uartioRx.serioRxEnbl = false;                    
                    ipIdx = 0;
                }
            }
        }
    }
    if (!uartioRx.serioRxEnbl) uart_set_irq_enables(UART_HW, false, false);
}

void uartIO_init( uint8_t id, uartIO_rxCallBack_t *rxCallBack, uint8_t *ipBuf, size_t ipMaxLen ) {
    uart_init(UART_HW, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(UART_HW, false, false);
    uart_set_format(UART_HW, DATA_BITS, STOP_BITS, PARITY);
    uart_set_fifo_enabled(UART_HW, false);

    uartioRx.serioRxId   = id;
    uartioRx.serioRxFn   = rxCallBack;
    uartioRx.serioRxBuf  = ipBuf;
    uartioRx.serioRxMax  = ipMaxLen;
    uartioRx.serioRxEnbl = false;

    int UART_IRQ = UART_HW == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, uartIO_rxCallBack);
    irq_set_enabled(UART_IRQ, true);

    bi_decl(bi_2pins_with_func(UART_RX_PIN, UART_TX_PIN, GPIO_FUNC_UART));
}
void uartIO_rxEnable(bool isEnabled) {
    uartioRx.serioRxEnbl = isEnabled;
    if (uartioRx.serioRxEnbl) uart_set_irq_enables(UART_HW, true, false);
    else                      uart_set_irq_enables(UART_HW, false, false);
}
void uartIO_buffSend(const uint8_t * opBuf, size_t opLen) {
    uart_write_blocking(UART_HW, opBuf, opLen);
}
