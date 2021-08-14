#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "string.h"

#include "queues_for_msgs_and_bits.h"

void putNxtMsg( volatile msgQue_t * msgQ ) {
    uint32_t flags = spin_lock_blocking( msgQ->mQueQLock );
    if (msgQ->mQueMsgCntr >= msgQ->mQueMaxRecs) {
        msgQ->mQueMsgOvrRun++;
        msgQ->mQueMsgCntr = msgQ->mQueMaxRecs;
    } else {
        if (++msgQ->mQueMsgHead >= msgQ->mQueMaxRecs) msgQ->mQueMsgHead = 0;
        msgQ->mQueMsgCntr++;
        if (msgQ->mQueMsgCntr > msgQ->mQueMsgHiWater) msgQ->mQueMsgHiWater = msgQ->mQueMsgCntr;
    }
    spin_unlock(msgQ->mQueQLock, flags);
}
void freeLastMsg( volatile msgQue_t * msgQ ) {
    uint32_t flags = spin_lock_blocking( msgQ->mQueQLock );
    if (msgQ->mQueMsgCntr > 0) {
        msgQ->mQueMsgCntr--;
        if (++msgQ->mQueMsgTail >= msgQ->mQueMaxRecs) msgQ->mQueMsgTail = 0;
    } else {
        msgQ->mQueMsgUndRun++;
    }
    spin_unlock(msgQ->mQueQLock, flags);
}
bool tryMsgBuf( volatile msgQue_t * msgQ ) {
    return msgQ->mQueMsgCntr > 0; 
}

/*-----------------------------------------------------------------*/
void putNxtWrd( volatile bitQue_t * bitQ, uint32_t nxtWrd ) {
    volatile uint32_t * wrdsBuf  = (volatile uint32_t *)bitQ->bQueWrdsBuffer;
    wrdsBuf[bitQ->bQueWrdHead] = nxtWrd;
    if (++bitQ->bQueWrdHead >= bitQ->bQueWrdsMax) bitQ->bQueWrdHead = 0;
    if (bitQ->bQueWrdCntr >= bitQ->bQueWrdsMax) {
        bitQ->bQueWrdOvrRun++;
        bitQ->bQueWrdCntr = bitQ->bQueWrdsMax;
        if (++bitQ->bQueWrdTail >= bitQ->bQueWrdsMax) bitQ->bQueWrdTail = 0;
     } else {
        bitQ->bQueWrdCntr++;
        if (bitQ->bQueWrdCntr > bitQ->bQueWrdHiWater) bitQ->bQueWrdHiWater = bitQ->bQueWrdCntr;
    }
 }
uint32_t getNxtWrd( volatile bitQue_t * bitQ ) {
    volatile uint32_t * wrdsBuf  = (volatile uint32_t *)bitQ->bQueWrdsBuffer;
    uint wrdOffSet = bitQ->bQueWrdTail;
    if (++bitQ->bQueWrdTail >= bitQ->bQueWrdsMax) bitQ->bQueWrdTail = 0;
    if (bitQ->bQueWrdCntr > 0) bitQ->bQueWrdCntr--;
                          else bitQ->bQueWrdUndRun++;
    return wrdsBuf[wrdOffSet];
}
bool tryWrdBuf( volatile bitQue_t * bitQ ) {
    return bitQ->bQueWrdCntr > 0; 
}
bool poll_FIFO_callback(struct repeating_timer *t) {
    volatile bitQue_t * bitQ = (volatile bitQue_t *)t->user_data;
    uint fifoCntr = 0;
    while (!pio_sm_is_rx_fifo_empty(bitQ->bQue_pio_id, bitQ->bQue_sm_id)) {
        putNxtWrd( bitQ, pio_sm_get(bitQ->bQue_pio_id, bitQ->bQue_sm_id) );
        fifoCntr++;
    }
    if (fifoCntr > bitQ->bQueFiFoHiWater) bitQ->bQueFiFoHiWater = fifoCntr;
    return true;
}
bool tryBitBuf( volatile bitQue_t * bitQ ) {
    if (bitQ->bQueBitsCntr == 0) {
        if (tryWrdBuf( bitQ )) {
            bitQ->bQueBitsBuffer = getNxtWrd( bitQ );
            bitQ->bQueBitsCntr = 32;
            bitQ->bQueTotBitsCntr++;
            for (uint i = 0; i < 32; i++)
                if (bitQ->bQueBitsBuffer & ((uint32_t)1U << i)) bitQ->bQueOneBitsCntr++; 
            uint32_t tNow  = to_ms_since_boot( get_absolute_time() );
            uint32_t tDiff = tNow - bitQ->bQueSmplTimStamp;
            if (tDiff > MSGQUE_BIT_PERIOD_SECS*1000) {
                bitQ->bQueSmplTotCnt  = bitQ->bQueTotBitsCntr;
                bitQ->bQueSmplOnesCnt = bitQ->bQueOneBitsCntr;
                bitQ->bQueSmplTimDlta = tDiff;
                bitQ->bQueSmplTimStamp= tNow;
                bitQ->bQueTotBitsCntr = 0;
                bitQ->bQueOneBitsCntr = 0;
            }
        }
    }
    return bitQ->bQueBitsCntr > 0;
}
bool getNxtBit_isSet( volatile bitQue_t * bitQ ) {
    bool isBitSet = (bitQ->bQueBitsBuffer & (uint32_t)0x80000000) != 0;
    bitQ->bQueBitsBuffer <<= 1;
    if (bitQ->bQueBitsCntr > 0) bitQ->bQueBitsCntr--;
                           else bitQ->bQueBitsUndRun++;
    return isBitSet;
}

/*-----------------------------------------------------------------*/
void msgChkPrevAndQue( bool chkPass, uint8_t sndrIdx, uint8_t chkSumRxd, volatile uint8_t * msgP, int len, 
                       volatile msgQue_t * msgQ, volatile msgRec_t * msgRecP )
{
    sender_t * sndrP = &msgQ->mQueSenders[0];
    bool doPutNxtMsg = false;
    bool prevSame = false;
    uint32_t tNow  = to_ms_since_boot( get_absolute_time() );
    uint32_t tDiff = tNow - msgQ->mQueSmplTimStamp;
    if (chkPass) {
        msgRecP->mRecSndrIdx = sndrIdx;
        msgRecP->mRecSndrId  = msgQ->mQueSndrId;
        if (msgQ->mQueHadPrv && (msgQ->mQueSndrId == msgQ->mQuePrevID) &&
                               (msgQ->mQuePrvChkSum == chkSumRxd) ) {
            prevSame = memcmp(&msgQ->mQuePrvMsgByts[0],(const void *)&msgP[0],len) == 0;
            if (!prevSame) {
                // e.g. the following have valid checksums of FF but are invalid WH1080 messages 
                // FFBFDEFFFFFFFFF7FFFFFF,FFBFDEBFEFEFFFDDFFFFFF,FFBFDDFFFFBFFBFEFFFFFF,FFBFDBFDFFFFFFFFEBFFFF
                msgQ->mQuePrvChkSumInvld++;
                if (msgQ->mQuePrvChkSum != 0xFF) printf("Not always 0xFF - chk %02X\n",msgQ->mQuePrvChkSum);
            }
        }
        if (prevSame) {
            doPutNxtMsg = true;
            msgRecP->mRecMsgVrfdDltaTim = tNow - sndrP[sndrIdx].sndrTimeStamp;
            sndrP[sndrIdx].sndrTimeStamp= tNow;
            sndrP[sndrIdx].sndrVrfyCnt++;
            msgQ->mQueHadPrv = false;
        } else {
            sndrP[sndrIdx].sndrUnVrfyCnt++;
            msgQ->mQueHadPrv = true;
            memcpy(&msgQ->mQuePrvMsgByts[0],(const void *)&msgP[0],len);
        }
        msgQ->mQuePrvChkSum = chkSumRxd;
        msgQ->mQuePrevID = msgQ->mQueSndrId;
    } else {
        msgQ->mQueChkErrs++;
        msgQ->mQueHadPrv = false;
    }
    if (tDiff > MSGQUE_SMPL_PERIOD_MINS*60*1000) {
        msgQ->mQueSmplTimStamp= tNow;
        msgQ->mQueSmplTimDlta = tDiff;
        msgQ->mQueSmplHdrHitsCnt = msgQ->mQueHdrHits;
        msgQ->mQueSmplChkErrsCnt = msgQ->mQueChkErrs;
        msgQ->mQueHdrHits = 0;
        msgQ->mQueChkErrs  = 0;
        for (uint sndrCntr = 0; sndrCntr < msgQ->mQueNumSndrs; sndrCntr++) {
            sndrP[sndrCntr].sndrSmplVrfyCnt   = sndrP[sndrCntr].sndrVrfyCnt;
            sndrP[sndrCntr].sndrSmplUnVrfyCnt = sndrP[sndrCntr].sndrUnVrfyCnt;
            sndrP[sndrCntr].sndrVrfyCnt   = 0;
            sndrP[sndrCntr].sndrUnVrfyCnt = 0;
        }
    }
    if (doPutNxtMsg) {
        volatile bitQue_t * bitQ = msgQ->mQueBitQueP;
        msgRecP->mRecBitSmplTimDlta = bitQ->bQueSmplTimDlta;
        msgRecP->mRecBitSmplTotCnt  = bitQ->bQueSmplTotCnt;
        msgRecP->mRecBitSmplOnesCnt = bitQ->bQueSmplOnesCnt;
        msgRecP->mRecMsgSmplTimStamp    = msgQ->mQueSmplTimStamp;
        msgRecP->mRecMsgSmplTimDlta     = msgQ->mQueSmplTimDlta;
        msgRecP->mRecMsgSmplHdrHitsCnt  = msgQ->mQueSmplHdrHitsCnt;
        msgRecP->mRecMsgSmplChkErrsCnt  = msgQ->mQueSmplChkErrsCnt;
        msgRecP->mRecMsgTimeStamp       = sndrP[sndrIdx].sndrTimeStamp;
        msgRecP->mRecMsgSmplVrfyCnt     = sndrP[sndrIdx].sndrSmplVrfyCnt;
        msgRecP->mRecMsgSmplUnVrfyCnt   = sndrP[sndrIdx].sndrSmplUnVrfyCnt;
        putNxtMsg(msgQ);
    }
}