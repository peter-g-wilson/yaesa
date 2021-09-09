#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

#include "queues_for_msgs_and_bits.h"
#include "output_format.h"
#include "proj_board.h"

const uint8_t dash_padding[] = "------------------------------------------------------------------------";
uint8_t OpMsgSeqNum = 0;

void print_msg( outBuff_t outBuff, outArgs_t * outArgsP ) {
    uint    totSmplMsgs = outArgsP->oArgMsgSmplVrfyCnt + outArgsP->oArgMsgSmplUnVrfyCnt;
    uint    totGoodHits = outArgsP->oArgMsgSmplHdrHitsCnt - outArgsP->oArgMsgSmplChkErrsCnt;
    uint        totBits = outArgsP->oArgBitSmplTotCnt*32;
    float  vrfdSmplPrct = totSmplMsgs == 0 ?
                          0.0F : (float)outArgsP->oArgMsgSmplVrfyCnt*100.0F/(float)totSmplMsgs;
    float  hitsGoodPrct = outArgsP->oArgMsgSmplHdrHitsCnt == 0 ?
                          0.0F : (float)totGoodHits*100.0F/(float)outArgsP->oArgMsgSmplHdrHitsCnt;
    float  onesBitsPrct = totBits == 0 ?
                          0.0F : (float)outArgsP->oArgBitSmplOnesCnt*100.0F/(float)totBits;

    float  msgDlta = (float)outArgsP->oArgMsgVrfdDltaTim*0.001F;
    float  msgRate;
    float  hdrRate;
    if (outArgsP->oArgMsgSmplTimDlta == 0) {
        msgRate = 0.0F;
        hdrRate = 0.0F;
    } else {
        float smplTimDlta = ((float)outArgsP->oArgMsgSmplTimDlta*0.001F)/((float)MSGQUE_SMPL_PERIOD_MINS*60.0F);
        msgRate = (float)outArgsP->oArgMsgSmplVrfyCnt / smplTimDlta;
        hdrRate = (float)outArgsP->oArgMsgSmplHdrHitsCnt / smplTimDlta;
    }
    float    bitRateSec = outArgsP->oArgBitSmplTimDlta == 0 ? 
                          0.0F : (float)totBits/((float)outArgsP->oArgBitSmplTimDlta*0.001F);
    printf( 
        "%08X %*.*s "\
        "Msg %1X %07.1f s %07.1f %05.1f %% "\
        "Hdr %07.1f %05.1f %% "\
        "Bit %07.1f /s %05.1f %% "\
        "HiWtr %02d %02d %02d Chk %02d\n",
        outArgsP->oArgMsgSmplTimStamp, outArgsP->oArgMsgLen-3, outArgsP->oArgMsgLen-3, outBuff,
        outArgsP->oArgSndrId,    msgDlta,  msgRate,  vrfdSmplPrct,
        hdrRate,                hitsGoodPrct,
        bitRateSec,             onesBitsPrct,
        outArgsP->oArgMsgHiWtr, outArgsP->oArgWrdHiWtr, outArgsP->oArgFiFoHiWtr, outArgsP->oArgPrvChkInvld ); 
}

void output_copy_args( int len, uint8_t id, volatile msgQue_t * msgQ, volatile msgRec_t * msgRecP, outArgs_t * outArgsP)
{
    volatile bitQue_t * bitQ = msgQ->mQueBitQueP;

    outArgsP->oArgMsgLen = len;
    
    outArgsP->oArgSndrId = id;
    outArgsP->oArgMsgVrfdDltaTim = msgRecP->mRecMsgVrfdDltaTim;

    outArgsP->oArgMsgSmplTimStamp   = msgRecP->mRecMsgSmplTimStamp;
    outArgsP->oArgMsgSmplTimDlta    = msgRecP->mRecMsgSmplTimDlta;
    outArgsP->oArgMsgSmplVrfyCnt    = msgRecP->mRecMsgSmplVrfyCnt;
    outArgsP->oArgMsgSmplUnVrfyCnt  = msgRecP->mRecMsgSmplUnVrfyCnt;
    outArgsP->oArgMsgSmplHdrHitsCnt = msgRecP->mRecMsgSmplHdrHitsCnt;
    outArgsP->oArgMsgSmplChkErrsCnt = msgRecP->mRecMsgSmplChkErrsCnt;

    outArgsP->oArgBitSmplTimDlta = msgRecP->mRecBitSmplTimDlta;
    outArgsP->oArgBitSmplTotCnt  = msgRecP->mRecBitSmplTotCnt;
    outArgsP->oArgBitSmplOnesCnt = msgRecP->mRecBitSmplOnesCnt;

    outArgsP->oArgMsgHiWtr  = msgQ->mQueMsgHiWater;
    outArgsP->oArgWrdHiWtr  = bitQ->bQueWrdHiWater;
    outArgsP->oArgFiFoHiWtr = bitQ->bQueFiFoHiWater; 
    outArgsP->oArgPrvChkInvld = msgQ->mQuePrvChkSumInvld;
}