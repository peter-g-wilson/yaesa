
#ifndef F007T_DECODE_BITS_H

#define F007T_MAXMSGBYTS  6

extern void F007T_init( uint32_t parseRptTime, uint32_t fifoRptTime );
extern void F007T_enable( void );
extern void F007T_uninit( void );
extern bool F007T_tryMsgBuf( uint32_t * tStampP );
extern int  F007T_doMsgBuf( void );
extern const uint8_t F007TlsfrMask[(F007T_MAXMSGBYTS-1)*8];

#define F007T_DECODE_BITS_H
#endif
