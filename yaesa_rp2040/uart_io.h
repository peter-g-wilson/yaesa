
#ifndef UART_IO_H

typedef void (*uartIO_rxCallBack_t)( int  );

extern void uartIO_init( uartIO_rxCallBack_t *rxCallBack, uint8_t *ipBuf, size_t ipMaxLen );
extern void uartIO_buffSend(const uint8_t *opBuf, size_t opLen);
extern void uartIO_rxEnable(bool isEnabled);

#define UART_IO_H
#endif
