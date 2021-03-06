
#ifndef WH1080_DECODE_BITS_H

extern void WH1080_init( uint32_t parseRptTime, uint32_t fifoRptTime );
extern void WH1080_enable( void );
extern void WH1080_uninit( void );
extern bool WH1080_tryMsgBuf( uint32_t * tStampP );
extern int WH1080_doMsgBuf( void );

#define WH1080_DECODE_BITS_H
#endif
