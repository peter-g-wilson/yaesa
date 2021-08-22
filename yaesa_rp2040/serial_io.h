
#ifndef SERIAL_IO_H

typedef int (*uartIO_rxCallBack_t)( uint8_t, uint8_t *, int  );

extern void uartIO_init( uint8_t id, uartIO_rxCallBack_t *rxCallBack, uint8_t *ipBuf, size_t ipMaxLen );
extern void uartIO_buffSend(const uint8_t *opBuf, size_t opLen);
extern void uartIO_rxEnable(bool isEnabled);

extern void stdioRx_init( uint8_t id, uartIO_rxCallBack_t *rxCallBack, uint8_t *ipBuf, size_t ipMaxLen );
extern void stdioRx_enable(bool isEnabled);

#define SERIAL_IO_H
#endif
