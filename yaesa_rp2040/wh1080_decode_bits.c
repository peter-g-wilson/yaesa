#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/binary_info.h"

#include "wh1080_pwmpulsebits.pio.h"
#include "queues_for_msgs_and_bits.h"
#include "wh1080_decode_bits.h"
#include "output_format.h"
#include "serial_io.h"
#include "string.h"
#include "proj_board.h"
#include "sched_ms.h"

/*-----------------------------------------------------------------*/
#define MAX_WH1080_BUFWRDS 32
volatile uint32_t WH1080rxWrdsBuf[MAX_WH1080_BUFWRDS];

volatile bitQue_t WH1080bitQ = {
    .bQue_pio_id   = WH1080_PIO_HW,
    .bQue_sm_id    = WH1080_SM_HW,
    .bQue_pin_rx   = WH1080_GPIO_RX,
    .bQueWrdsMax   = MAX_WH1080_BUFWRDS,
    .bQueWrdsBuffer= (void *)&WH1080rxWrdsBuf[0]
};  

#define MAX_WH1080_SENDERS  1
sender_t WH1080senders[MAX_WH1080_SENDERS];

#define WH1080_MAXMSGBYTS  11
#define MAX_WH1080_BUFMSGS 16

uint8_t  msgPrvWH1080[WH1080_MAXMSGBYTS];

uint8_t mRecBuffWH1080[MAX_WH1080_BUFMSGS*WH1080_MAXMSGBYTS];

msgRec_t msgRecsWH1080[MAX_WH1080_BUFMSGS];

volatile msgQue_t WH1080msgQ = {
    .mQueBitQueP   = &WH1080bitQ,
    .mQueMaxRecs   = MAX_WH1080_BUFMSGS,
    .mQueNumSndrs  = MAX_WH1080_SENDERS,
    .mQueSenders   = &WH1080senders[0],
    .mQueRecByts   = &mRecBuffWH1080[0],
    .mQueRecStats  = &msgRecsWH1080[0],
    .mQuePrvMsgByts= &msgPrvWH1080[0],
};

/*-----------------------------------------------------------------*/
void parseWH1080bits_callback(void * msgQp) {
    static const uint8_t crctab[256] = {
        0x00, 0x31, 0x62, 0x53, 0xC4, 0xF5, 0xA6, 0x97, 0xB9, 0x88, 0xDB, 0xEA, 0x7D, 0x4C, 0x1F, 0x2E,
        0x43, 0x72, 0x21, 0x10, 0x87, 0xB6, 0xE5, 0xD4, 0xFA, 0xCB, 0x98, 0xA9, 0x3E, 0x0F, 0x5C, 0x6D,
        0x86, 0xB7, 0xE4, 0xD5, 0x42, 0x73, 0x20, 0x11, 0x3F, 0x0E, 0x5D, 0x6C, 0xFB, 0xCA, 0x99, 0xA8,
        0xC5, 0xF4, 0xA7, 0x96, 0x01, 0x30, 0x63, 0x52, 0x7C, 0x4D, 0x1E, 0x2F, 0xB8, 0x89, 0xDA, 0xEB,
        0x3D, 0x0C, 0x5F, 0x6E, 0xF9, 0xC8, 0x9B, 0xAA, 0x84, 0xB5, 0xE6, 0xD7, 0x40, 0x71, 0x22, 0x13,
        0x7E, 0x4F, 0x1C, 0x2D, 0xBA, 0x8B, 0xD8, 0xE9, 0xC7, 0xF6, 0xA5, 0x94, 0x03, 0x32, 0x61, 0x50,
        0xBB, 0x8A, 0xD9, 0xE8, 0x7F, 0x4E, 0x1D, 0x2C, 0x02, 0x33, 0x60, 0x51, 0xC6, 0xF7, 0xA4, 0x95,
        0xF8, 0xC9, 0x9A, 0xAB, 0x3C, 0x0D, 0x5E, 0x6F, 0x41, 0x70, 0x23, 0x12, 0x85, 0xB4, 0xE7, 0xD6,
        0x7A, 0x4B, 0x18, 0x29, 0xBE, 0x8F, 0xDC, 0xED, 0xC3, 0xF2, 0xA1, 0x90, 0x07, 0x36, 0x65, 0x54,
        0x39, 0x08, 0x5B, 0x6A, 0xFD, 0xCC, 0x9F, 0xAE, 0x80, 0xB1, 0xE2, 0xD3, 0x44, 0x75, 0x26, 0x17,
        0xFC, 0xCD, 0x9E, 0xAF, 0x38, 0x09, 0x5A, 0x6B, 0x45, 0x74, 0x27, 0x16, 0x81, 0xB0, 0xE3, 0xD2,
        0xBF, 0x8E, 0xDD, 0xEC, 0x7B, 0x4A, 0x19, 0x28, 0x06, 0x37, 0x64, 0x55, 0xC2, 0xF3, 0xA0, 0x91,
        0x47, 0x76, 0x25, 0x14, 0x83, 0xB2, 0xE1, 0xD0, 0xFE, 0xCF, 0x9C, 0xAD, 0x3A, 0x0B, 0x58, 0x69,
        0x04, 0x35, 0x66, 0x57, 0xC0, 0xF1, 0xA2, 0x93, 0xBD, 0x8C, 0xDF, 0xEE, 0x79, 0x48, 0x1B, 0x2A,
        0xC1, 0xF0, 0xA3, 0x92, 0x05, 0x34, 0x67, 0x56, 0x78, 0x49, 0x1A, 0x2B, 0xBC, 0x8D, 0xDE, 0xEF,
        0x82, 0xB3, 0xE0, 0xD1, 0x46, 0x77, 0x24, 0x15, 0x3B, 0x0A, 0x59, 0x68, 0xFF, 0xCE, 0x9D, 0xAC
    };
#define WH1080_HDR_MASK    ((uint32_t)0x0003FEFF)
#define WH1080_HDR_MATCH   ((uint32_t)0x0003FAFD)
#define WH1080_HDR_FIXBITS ((uint32_t)0x000C0000)

    static uint32_t   header     = 0;
    static uint       waitMsgHdr = true;
    static uint       bytCnt;
    static uint       bitCnt;
    static uint8_t    chkSumCalc;
    static volatile uint8_t  * msgP;
    static volatile msgRec_t * msgRecP;
    volatile        msgQue_t * msgQ = (volatile msgQue_t *)msgQp;
    volatile        bitQue_t * bitQ = msgQ->mQueBitQueP;
    while (tryBitBuf( bitQ )) {
        bool nxtBitIsSet = getNxtBit_isSet( bitQ );
        if (waitMsgHdr) {
            header <<= 1;
            if (nxtBitIsSet) header |= 1;
            if ((uint32_t)(header & WH1080_HDR_MASK) == WH1080_HDR_MATCH) {
                msgQ->mQueHdrHits++;
                msgRecP    = &msgQ->mQueRecStats[msgQ->mQueMsgHead];
                msgP       = &msgQ->mQueRecByts[msgQ->mQueMsgHead*WH1080_MAXMSGBYTS];
                header    |= WH1080_HDR_FIXBITS;
                msgP[0]    = (uint8_t)((header & 0x000FF000) >> 12);
                msgP[1]    = (uint8_t)((header & 0x00000FF0) >> 4);
                msgP[2]    = (uint8_t)( header & 0x0000000F);
                chkSumCalc = 0;
                chkSumCalc = crctab[msgP[1] ^ chkSumCalc];
                bytCnt     = 2;
                bitCnt     = 4;
                header     = 0;
                waitMsgHdr = false;
            }            
        } else {
            msgP[bytCnt] <<= 1;
            if (nxtBitIsSet) msgP[bytCnt] |= 1;
            bitCnt++;
            if (bitCnt >= 8) {
                chkSumCalc = crctab[msgP[bytCnt] ^ chkSumCalc];
                bitCnt = 0;
                bytCnt++;
                if (bytCnt >= WH1080_MAXMSGBYTS) {
                    uint8_t sndrIdx = 0;
                    uint8_t chkSumRxd = msgP[WH1080_MAXMSGBYTS-1];
                    bool chkSumPass = chkSumCalc == 0;
                    if (chkSumPass) {
                        msgQ->mQueSndrId = 0xFD00 | ((msgP[1] & 0xF0) >> 4);
                    }
                    msgChkPrevAndQue( chkSumPass, sndrIdx, chkSumRxd, msgP, WH1080_MAXMSGBYTS, msgQ, msgRecP );
                    waitMsgHdr = true;
                }
            }
        }           
    }
}

/*-----------------------------------------------------------------*/
#define WH1080_HEXCODES_LEN (OFRMT_HEADER_LEN + WH1080_MAXMSGBYTS * 2)
#define WH1080_MAX_FD0B_FRMT 31
#define WH1080_MAX_FD0A_FRMT 49
#define WH1080_UNKWN_FRMT     1
#define WH1080_PREDASHPAD        (OFRMT_HEXCODS_LEN - WH1080_MAXMSGBYTS * 2)
#define WH1080_FD0B_POSTDASHPAD  (OFRMT_DECODES_LEN - WH1080_MAX_FD0B_FRMT)
#define WH1080_FD0A_POSTDASHPAD  (OFRMT_DECODES_LEN - WH1080_MAX_FD0A_FRMT)
#define WH1080_UNKWN_POSTDASHPAD (OFRMT_DECODES_LEN - WH1080_UNKWN_FRMT)

int decode_WH1080_msg( volatile msgQue_t * msgQ, outBuff_t outBuff, outArgs_t * outArgsP ) {
    volatile msgRec_t * msgRecP = &msgQ->mQueRecStats[msgQ->mQueMsgTail];
    volatile uint8_t  * msgP    = &msgQ->mQueRecByts[msgQ->mQueMsgTail*WH1080_MAXMSGBYTS];
    volatile bitQue_t * bitQ    = msgQ->mQueBitQueP;
    uint msgId = msgRecP->mRecSndrId;
    bool validVals = false;
    uint sndrIdx;

    int msgLen = snprintf( &outBuff[0], OFRMT_HEADER_LEN+1, "%0*d %0*X-%0*X-",
                           OFRMT_SEQ_NUM_LEN, OpMsgSeqNum, OFRMT_SNDR_ID_LEN, msgId, 
                                         OFRMT_TSTAMP_LEN, msgRecP->mRecMsgTimeStamp);
    OpMsgSeqNum++;
    for (uint i = 0; i < WH1080_MAXMSGBYTS; i++) {
        msgLen += snprintf( &outBuff[OFRMT_HEADER_LEN+(i*2)], 2+1, "%02X", msgP[i] );
    }
    if  (msgId == 0xFD0B) {
        uint8_t *sTyp = ((msgP[2] & 0x0F) == 10) ? "DCF77" : "?????"; // 5 
        uint     tHrs = ((msgP[3] & 0x30) >> 4)*10 + (msgP[3] & 0x0F);
        uint     tMin = ((msgP[4] & 0xF0) >> 4)*10 + (msgP[4] & 0x0F);
        uint     tSec = ((msgP[5] & 0xF0) >> 4)*10 + (msgP[5] & 0x0F);
        uint     tYrs = ((msgP[6] & 0xF0) >> 4)*10 + (msgP[6] & 0x0F) + 2000;
        uint     tMth = ((msgP[7] & 0x10) >> 4)*10 + (msgP[7] & 0x0F);
        uint     tDay = ((msgP[8] & 0xF0) >> 4)*10 + (msgP[8] & 0x0F);
        validVals = !( (tYrs > 2099) || (tMth < 1)  || (tMth > 12) || (tDay < 1) || (tDay > 31) || 
                         (tHrs > 23) || (tMin > 59) || (tSec > 59) ); 
        if (validVals) {
            sndrIdx = 0x0B;
            msgLen += snprintf( &outBuff[WH1080_HEXCODES_LEN], OFRMT_TOTAL_LEN - WH1080_HEXCODES_LEN + 1,
                   "%*.*s-i:%1X,s:%5.5s,%04d-%02d-%02d,%02d:%02d:%02d%*.*s",
                   WH1080_PREDASHPAD,  WH1080_PREDASHPAD,  dash_padding,
                   sndrIdx, sTyp, tYrs, tMth, tDay, tHrs, tMin, tSec,
                   WH1080_FD0B_POSTDASHPAD, WH1080_FD0B_POSTDASHPAD, dash_padding);
        }
    } else if (msgId == 0xFD0A) {
        uint16_t tempRaw    = ((msgP[2] & 0x0F) << 8) | 
                                msgP[3];
        uint8_t  humid      =   msgP[4];
        uint8_t  wndAvg     =   msgP[5];
        uint8_t  wndGst     =   msgP[6];
        uint16_t rainRaw    = ((msgP[7] & 0x0F) << 8) |
                                msgP[8];
        bool     battSts    =  (msgP[9] & 0xF0) > 0;
        uint8_t  wndDir     =   msgP[9] & 0x0F;
        float    tempDegC   = ((float)((int16_t)tempRaw - 400))*0.1F;
        float    rain_mm    =  (float)rainRaw*0.3F;
        float    wndAvg_mps =  (float)wndAvg*0.34F;
        float    wndGst_mps =  (float)wndGst*0.34F;

        if (tempDegC   < -99.0F)  tempDegC =  -99.0F;
        if (rain_mm    > 9999.0F)  rain_mm = 9999.0F;
        if (wndAvg_mps > 99.9F) wndAvg_mps =   99.9F;
        if (tempDegC   > 999.0F)  tempDegC =  999.0F;
        if (wndGst_mps > 99.9F) wndGst_mps =   99.9F;
        validVals = true;
        sndrIdx = 0x0A;

        msgLen += snprintf( &outBuff[WH1080_HEXCODES_LEN], OFRMT_TOTAL_LEN - WH1080_HEXCODES_LEN + 1,
               "%*.*s-i:%1X,b:%1d,t:%05.1f,h:%03d,r:%06.1f,a:%04.1f,g:%04.1f,c:%02d%*.*s",
               WH1080_PREDASHPAD,  WH1080_PREDASHPAD,  dash_padding,
               sndrIdx, battSts, tempDegC, humid, rain_mm, wndAvg_mps, wndGst_mps, wndDir,
               WH1080_FD0A_POSTDASHPAD, WH1080_FD0A_POSTDASHPAD, dash_padding);

    }
    if (!validVals) {
        sndrIdx = 0x0C;
        msgLen += snprintf( &outBuff[WH1080_HEXCODES_LEN], OFRMT_TOTAL_LEN - WH1080_HEXCODES_LEN + 1,
               "%*.*s-?%*.*s",
               WH1080_PREDASHPAD, WH1080_PREDASHPAD, dash_padding,
               WH1080_UNKWN_POSTDASHPAD, WH1080_UNKWN_POSTDASHPAD, dash_padding);
    }
    msgLen += 3;
    OFRMT_PRINT_PATHETIC_EXCUSE(msgId,msgLen);

    outBuff[msgLen-3] = 13;
    outBuff[msgLen-2] = 10;
    outBuff[msgLen-1] = 0;
    
    output_copy_args( msgLen, msgId & 0x000F, msgQ, msgRecP, outArgsP);
    return sndrIdx;
}

/*-----------------------------------------------------------------*/
outBuff_t WH1080msgcods;

bool WH1080_tryMsgBuf( uint32_t * tStampP ) {
    return tryMsgBuf( &WH1080msgQ, tStampP );;
}

int WH1080_doMsgBuf( void ) {
    outArgs_t outArgs;
    uint sndrIdx = decode_WH1080_msg( &WH1080msgQ, WH1080msgcods, &outArgs );
    freeLastMsg( &WH1080msgQ );
    uartIO_buffSend(&WH1080msgcods[0],outArgs.oArgMsgLen);
    print_msg( WH1080msgcods, &outArgs );
    return sndrIdx;
}
/*-----------------------------------------------------------------*/
void WH1080_init( uint32_t parseRptTime, uint32_t fifoRptTime ) {
    volatile msgQue_t * msgQ = &WH1080msgQ;
    volatile bitQue_t * bitQ = msgQ->mQueBitQueP;
    //gpio_init(                        bitQ->bQue_pin_rx );
    //gpio_set_dir(                     bitQ->bQue_pin_rx, GPIO_IN );
    //gpio_pull_down(                   bitQ->bQue_pin_rx );
    pio_gpio_init( bitQ->bQue_pio_id, bitQ->bQue_pin_rx );

    uint offset_prog = pio_add_program( bitQ->bQue_pio_id, &PWMpulseBits_program );
    pio_sm_config cfg_prog = PWMpulseBits_program_get_default_config( offset_prog );    

    sm_config_set_in_pins(   &cfg_prog, bitQ->bQue_pin_rx );
    sm_config_set_jmp_pin(   &cfg_prog, bitQ->bQue_pin_rx );
    sm_config_set_in_shift(  &cfg_prog, false, true, 0 );
    sm_config_set_fifo_join( &cfg_prog, PIO_FIFO_JOIN_RX );
    sm_config_set_clkdiv(    &cfg_prog, 625.0F  );

    pio_sm_init(        bitQ->bQue_pio_id, bitQ->bQue_sm_id, offset_prog, &cfg_prog );
    pio_sm_clear_fifos( bitQ->bQue_pio_id, bitQ->bQue_sm_id);
    pio_sm_set_enabled( bitQ->bQue_pio_id, bitQ->bQue_sm_id, true);

    msgQ->mQueQLock = spin_lock_instance( next_striped_spin_lock_num() );
    
    sched_init_slot(SCHED_CORE0_SLOT3,parseRptTime, parseWH1080bits_callback, (void *)&WH1080msgQ);
    sched_init_slot(SCHED_CORE0_SLOT1,fifoRptTime,  poll_FIFO_callback,       (void *)&WH1080bitQ);

    bi_decl(bi_1pin_with_name(WH1080_GPIO_RX, WH1080_CONFIG));
}
void WH1080_uninit( void ) {
    volatile msgQue_t * msgQ = &WH1080msgQ;
    volatile bitQue_t * bitQ = msgQ->mQueBitQueP;
    pio_sm_set_enabled( bitQ->bQue_pio_id, bitQ->bQue_sm_id, false );
}
