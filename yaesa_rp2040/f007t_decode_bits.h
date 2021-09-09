
#ifndef F007T_DECODE_BITS_H

extern void F007T_init( uint32_t parseRptTime, uint32_t fifoRptTime );
extern void F007T_uninit( void );
extern bool F007T_tryMsgBuf( uint32_t * tStampP );
extern int F007T_doMsgBuf( void );

#define F007T_DECODE_BITS_H
#endif
