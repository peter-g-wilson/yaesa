
#ifndef WH1080_DECODE_BITS_H

extern void WH1080_init( uint32_t parseRptTime, uint32_t fifoRptTime );
extern void WH1080_uninit( void );
extern bool WH1080_tryMsgBuf( void );
extern int WH1080_doMsgBuf( void );

#define WH1080_DECODE_BITS_H
#endif
