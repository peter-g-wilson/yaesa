#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "pico/binary_info.h"

#include "proj_board.h"
#include "uart_io.h"

#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

uartIO_rxCallBack_t *rxCallBackFn;
uint8_t * rxCallBackIpBuff;
size_t rxCallBackIpMaxLen;
bool rxEnabled = false;

void uartIO_on_rx() {
    static size_t ipIdx = 0;
    while (rxEnabled && uart_is_readable(UART_HW)) {
        if (ipIdx < rxCallBackIpMaxLen) {
            uint8_t ch = uart_getc(UART_HW);
            if (ch < 128) {
                rxCallBackIpBuff[ipIdx++] = ch;
                if ((ch == 10) || (ipIdx >= rxCallBackIpMaxLen)) {
                    void (*fp) (int) = (void *) rxCallBackFn;
                    fp( ipIdx );
                    rxEnabled = false;                    
                    ipIdx = 0;
                }
            }
        }
    }
    if (!rxEnabled) uart_set_irq_enables(UART_HW, false, false);
}

void uartIO_init( uartIO_rxCallBack_t *rxCallBack, uint8_t *ipBuf, size_t ipMaxLen ) {
    uart_init(UART_HW, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(UART_HW, false, false);
    uart_set_format(UART_HW, DATA_BITS, STOP_BITS, PARITY);
    uart_set_fifo_enabled(UART_HW, false);

    rxCallBackFn = rxCallBack;
    rxCallBackIpBuff = ipBuf;
    rxCallBackIpMaxLen = ipMaxLen;

    int UART_IRQ = UART_HW == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, uartIO_on_rx);
    irq_set_enabled(UART_IRQ, true);

    bi_decl(bi_2pins_with_func(UART_RX_PIN, UART_TX_PIN, GPIO_FUNC_UART));
}
void uartIO_rxEnable(bool isEnabled) {
    rxEnabled = isEnabled;
    if (rxEnabled) uart_set_irq_enables(UART_HW, true, false);
    else           uart_set_irq_enables(UART_HW, false, false);
}
void uartIO_buffSend(const uint8_t * opBuf, size_t opLen) {
    uart_write_blocking(UART_HW, opBuf, opLen);
}
