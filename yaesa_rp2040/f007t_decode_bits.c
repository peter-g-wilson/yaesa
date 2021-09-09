#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "pico/binary_info.h"

#include "f007t_manchwithdelay.pio.h"
#include "queues_for_msgs_and_bits.h"
#include "f007t_decode_bits.h"
#include "output_format.h"
#include "serial_io.h"
#include "string.h"
#include "proj_board.h"
#include "sched_ms.h"

#define MAX_F007T_BUFWRDS 32
volatile uint32_t F007TrxWrdsBuf[MAX_F007T_BUFWRDS];

volatile bitQue_t F007TbitQ = {
    .bQue_pio_id   = F007T_PIO_HW,
    .bQue_sm_id    = F007T_SM_HW,
    .bQue_pin_rx   = F007T_GPIO_RX,
    .bQueWrdsMax   = MAX_F007T_BUFWRDS,
    .bQueWrdsBuffer= (void *)&F007TrxWrdsBuf[0]
};  

#define MAX_F007T_SENDERS   8
#define MAX_F007T_KNWNSNDRIDX 4
sender_t F007Tsenders[MAX_F007T_SENDERS];

#define F007T_MAXMSGBYTS  6
#define MAX_F007T_BUFMSGS 16

uint8_t  msgPrvF007T[F007T_MAXMSGBYTS];

uint8_t mRecBuffF007T[MAX_F007T_BUFMSGS*F007T_MAXMSGBYTS];

msgRec_t msgRecsF007T[MAX_F007T_BUFMSGS];

volatile msgQue_t F007TmsgQ = {
    .mQueBitQueP   = &F007TbitQ,
    .mQueMaxRecs   = MAX_F007T_BUFMSGS,
    .mQueNumSndrs  = MAX_F007T_SENDERS,
    .mQueSenders   = &F007Tsenders[0],
    .mQueRecByts   = &mRecBuffF007T[0],
    .mQueRecStats  = (void *)&msgRecsF007T[0],
    .mQuePrvMsgByts= &msgPrvF007T[0],
};

/*-----------------------------------------------------------------*/
void parseF007Tbits_callback(void * msgQp) {
    const uint8_t lsfrMask[(F007T_MAXMSGBYTS-1)*8] = {
        0x3e, 0x1f, 0x97, 0xd3, 0xf1, 0xe0, 0x70, 0x38,
        0x1c, 0x0e, 0x07, 0x9b, 0xd5, 0xf2, 0x79, 0xa4,  
        0x52, 0x29, 0x8c, 0x46, 0x23, 0x89, 0xdc, 0x6e,
        0x37, 0x83, 0xd9, 0xf4, 0x7a, 0x3d, 0x86, 0x43,  
        0xb9, 0xc4, 0x62, 0x31, 0x80, 0x40, 0x20, 0x10
    };
#define F007T_HDR_MASK      ((uint32_t)0x000FFFFF)
#define F007T_HDR_46        ((uint32_t)0x00000046)
#define F007T_HDR_MATCH46  (((uint32_t)0x000FFD00) | F007T_HDR_46)
#define F007T_HDR_MATCH46X (((uint32_t)0x00000100) | F007T_HDR_46)

    static uint32_t header     = 0;
    static uint     waitMsgHdr = true;
    static uint     bytCnt;
    static uint     bitCnt;
    static uint8_t  chkSumCalc;
    static volatile uint8_t  * msgP;
    static volatile msgRec_t * msgRecP;
    volatile        msgQue_t * msgQ = (volatile msgQue_t *)msgQp;
    volatile        bitQue_t * bitQ = msgQ->mQueBitQueP;
    while (tryBitBuf( bitQ )) {
        bool nxtBitIsSet = getNxtBit_isSet( bitQ );
        if (waitMsgHdr) {
            header <<= 1;
            if (nxtBitIsSet) header |= 1;
            if ((uint32_t)(header & F007T_HDR_MASK) == F007T_HDR_MATCH46) {
                msgQ->mQueHdrHits++;
                msgRecP  = &msgQ->mQueRecStats[msgQ->mQueMsgHead];
                msgP     = &msgQ->mQueRecByts[msgQ->mQueMsgHead*F007T_MAXMSGBYTS];
                msgP[0]  = (uint8_t)(header & 0x000000FF);
                chkSumCalc = 100;
                for (uint8_t i = 0, m = 0x80; i < 8; i++, m >>= 1)
                    if (msgP[0] & m) chkSumCalc ^= lsfrMask[i];
                bytCnt     = 1;
                bitCnt     = 0;
                header     = 0;
                waitMsgHdr = false;
            }            
        } else {
            msgP[bytCnt] <<= 1;
            if (nxtBitIsSet) {
                msgP[bytCnt] |= 1;
                if (bytCnt < (F007T_MAXMSGBYTS-1)) {
                    chkSumCalc ^= lsfrMask[bytCnt*8+bitCnt];
                }
            }
            bitCnt++;
            if (bitCnt >= 8) {
                bitCnt = 0;
                bytCnt++;
                if (bytCnt >= F007T_MAXMSGBYTS) {
                    uint8_t sndrIdx = 0;
                    uint8_t chkSumRxd = msgP[F007T_MAXMSGBYTS-1];
                    bool chkSumPass = chkSumRxd == chkSumCalc;
                    if (chkSumPass) {
                        sndrIdx = ((msgP[2] >> 4) & 7);
                        msgQ->mQueSndrId = (msgP[0] << 8) | sndrIdx;
                    }
                    msgChkPrevAndQue( chkSumPass, sndrIdx, chkSumRxd, msgP, F007T_MAXMSGBYTS, msgQ, msgRecP );
                    waitMsgHdr = true;
                }
            }
        }           
    }
}

/*-----------------------------------------------------------------*/
#define F007T_HEXCODES_LEN (OFRMT_HEADER_LEN + F007T_MAXMSGBYTS * 2)
#define F007T_MAXMSGFRMT 15
#define F007T_PREDASHPAD  (OFRMT_HEXCODS_LEN - F007T_MAXMSGBYTS * 2)
#define F007T_POSTDASHPAD (OFRMT_DECODES_LEN - F007T_MAXMSGFRMT)

int decode_F007T_msg( volatile msgQue_t * msgQ, outBuff_t outBuff, outArgs_t * outArgsP ) {
    volatile msgRec_t * msgRecs = msgQ->mQueRecStats;
    volatile msgRec_t * msgRecP = &msgQ->mQueRecStats[msgQ->mQueMsgTail];
    volatile uint8_t  * msgP    = &msgQ->mQueRecByts[msgQ->mQueMsgTail*F007T_MAXMSGBYTS];
    volatile bitQue_t * bitQ    = msgQ->mQueBitQueP;
    uint  msgId   = msgRecP->mRecSndrId;
    uint  sndrIdx = msgRecP->mRecSndrIdx;
    bool  battLow =  (msgP[2] & 0x80) != 0;
    uint  tmpRaw  = ((msgP[2] & 0xF) << 8) | msgP[3];
    uint  tmpX2   = tmpRaw * 2u * 5u / 9u;
    int   tmpCX10 = tmpX2 & 1 ? (tmpX2+1)/2-400 : tmpX2/2-400;
    float tmpDegC = (float)tmpCX10 * 0.1F;

    if (tmpDegC < -99.0F) tmpDegC = -99.0F;
    if (tmpDegC > 999.0F) tmpDegC = 999.0F;

    int msgLen = snprintf( &outBuff[0], OFRMT_HEADER_LEN+1, "%0*d %0*X-%0*X-",
                            OFRMT_SEQ_NUM_LEN, OpMsgSeqNum, OFRMT_SNDR_ID_LEN, msgId, 
                                          OFRMT_TSTAMP_LEN, msgRecP->mRecMsgTimeStamp);
    OpMsgSeqNum++;
    for (uint i = 0; i < F007T_MAXMSGBYTS; i++) {
        msgLen += snprintf( &outBuff[OFRMT_HEADER_LEN+(i*2)], 2+1, "%02X", msgP[i] );
    }
    msgLen += snprintf( &outBuff[F007T_HEXCODES_LEN], OFRMT_TOTAL_LEN - F007T_HEXCODES_LEN + 1,
               "%*.*s-i%c%1d,b:%1d,t:%05.1f%*.*s",
               F007T_PREDASHPAD,  F007T_PREDASHPAD,  dash_padding,
               sndrIdx > MAX_F007T_KNWNSNDRIDX ? '?' : ':',sndrIdx+1,battLow,tmpDegC,
               F007T_POSTDASHPAD, F007T_POSTDASHPAD, dash_padding);

    msgLen += 3;
    OFRMT_PRINT_PATHETIC_EXCUSE(msgId,msgLen);

    outBuff[msgLen-3] = 13;
    outBuff[msgLen-2] = 10;
    outBuff[msgLen-1] = 0;

    output_copy_args( msgLen, sndrIdx + 1, msgQ, msgRecP, outArgsP);
    return sndrIdx;
}

/*-----------------------------------------------------------------*/
outBuff_t F007Tmsgcods;

bool F007T_tryMsgBuf( uint32_t * tStampP ) {
    return tryMsgBuf( &F007TmsgQ, tStampP );
}

int F007T_doMsgBuf( void ) {
    outArgs_t outArgs;
    int sndrIdx = decode_F007T_msg( &F007TmsgQ, F007Tmsgcods, &outArgs );
    freeLastMsg( &F007TmsgQ );
    uartIO_buffSend(&F007Tmsgcods[0],outArgs.oArgMsgLen);
    print_msg( F007Tmsgcods, &outArgs );
    return sndrIdx;
}

/*-----------------------------------------------------------------*/
void F007T_init( uint32_t parseRptTime, uint32_t fifoRptTime ) {
    volatile msgQue_t * msgQ = &F007TmsgQ;
    volatile bitQue_t * bitQ = msgQ->mQueBitQueP;
    //gpio_init(                        bitQ->bQue_pin_rx );
    //gpio_set_dir(                     bitQ->bQue_pin_rx, GPIO_IN );
    //gpio_pull_down(                   bitQ->bQue_pin_rx );
    pio_gpio_init( bitQ->bQue_pio_id, bitQ->bQue_pin_rx );

    uint offset_prog = pio_add_program( bitQ->bQue_pio_id, &manchWithDelay_program );
    pio_sm_config cfg_prog = manchWithDelay_program_get_default_config( offset_prog );    

    sm_config_set_in_pins(   &cfg_prog, bitQ->bQue_pin_rx );
    sm_config_set_jmp_pin(   &cfg_prog, bitQ->bQue_pin_rx );
    sm_config_set_in_shift(  &cfg_prog, false, true, 32 );
    sm_config_set_fifo_join( &cfg_prog, PIO_FIFO_JOIN_RX );
    sm_config_set_clkdiv(    &cfg_prog, 2543.0F  );

    pio_sm_init(        bitQ->bQue_pio_id, bitQ->bQue_sm_id, offset_prog, &cfg_prog );
    pio_sm_clear_fifos( bitQ->bQue_pio_id, bitQ->bQue_sm_id);
    pio_sm_exec(        bitQ->bQue_pio_id, bitQ->bQue_sm_id, pio_encode_set(pio_x, 1));
    pio_sm_set_enabled( bitQ->bQue_pio_id, bitQ->bQue_sm_id, true);

    msgQ->mQueQLock = spin_lock_instance( next_striped_spin_lock_num() );
    
    sched_init_slot(SCHED_CORE0_SLOT2, parseRptTime, parseF007Tbits_callback, (void *)&F007TmsgQ);
    sched_init_slot(SCHED_CORE0_SLOT0, fifoRptTime , poll_FIFO_callback,      (void *)&F007TbitQ);
    bi_decl(bi_1pin_with_name(F007T_GPIO_RX, F007T_CONFIG));
}
void F007T_uninit( void ) {
    volatile msgQue_t * msgQ = &F007TmsgQ;
    volatile bitQue_t * bitQ = msgQ->mQueBitQueP;
    pio_sm_set_enabled( bitQ->bQue_pio_id, bitQ->bQue_sm_id, false );
}
