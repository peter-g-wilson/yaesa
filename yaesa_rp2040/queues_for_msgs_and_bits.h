
#ifndef  QUEUES_FOR_MSGS_AND_BITS_H

#define MSGQUE_BIT_PERIOD_SECS     60
#define MSGQUE_SMPL_PERIOD_MINS  30

typedef struct senderStruct {
    uint32_t  sndrTimeStamp;
    uint      sndrVrfyCnt;
    uint      sndrUnVrfyCnt;
    uint      sndrSmplVrfyCnt;
    uint      sndrSmplUnVrfyCnt;
} sender_t;

typedef struct msgRecStruct {
    uint32_t  mRecMsgVrfdDltaTim;
    uint16_t  mRecSndrId;
    uint8_t   mRecSndrIdx;
    uint32_t  mRecMsgTimeStamp;    
    uint32_t  mRecMsgSmplTimStamp;    
    uint32_t  mRecMsgSmplTimDlta;    
    uint      mRecMsgSmplVrfyCnt;
    uint      mRecMsgSmplUnVrfyCnt;
    uint      mRecMsgSmplHdrHitsCnt;
    uint      mRecMsgSmplChkErrsCnt;
    uint32_t  mRecBitSmplTimDlta;    
    uint      mRecBitSmplTotCnt;
    uint      mRecBitSmplOnesCnt;
} msgRec_t;

typedef struct bitQueStruct {
    PIO        bQue_pio_id;
    const uint bQue_sm_id;
    const uint bQue_pin_rx;
    uint     bQueWrdTail;
    uint     bQueWrdHead;
    uint     bQueWrdCntr;
    uint     bQueWrdHiWater;
    uint     bQueWrdOvrRun;
    uint     bQueWrdUndRun;
    uint     bQueBitsUndRun;
    uint     bQueBitsCntr;
    uint32_t bQueBitsBuffer;
    uint     bQueFiFoHiWater;
    uint     bQueTotBitsCntr;
    uint     bQueOneBitsCntr;
    uint     bQueSmplTotCnt;
    uint     bQueSmplOnesCnt;
    uint32_t bQueSmplTimDlta;
    uint32_t bQueSmplTimStamp;
    uint     bQueWrdsMax;
    void   * bQueWrdsBuffer;
} bitQue_t;

typedef struct msgQueStruct {
    uint     mQueMsgTail;
    uint     mQueMsgHead;
    uint     mQueMsgCntr;
    uint     mQueMsgOvrRun;
    uint     mQueMsgUndRun;
    uint     mQueMsgHiWater;
    uint     mQueChkErrs;
    uint     mQueHdrHits;
    uint32_t mQueSmplTimStamp;
    uint32_t mQueSmplTimDlta;
    uint     mQueSmplVrfyCnt; 
    uint     mQueSmplUnVrfyCnt;
    uint     mQueSmplChkErrsCnt;
    uint     mQueSmplHdrHitsCnt;
    uint     mQueMaxRecs;
    uint     mQuePrvChkSumInvld;
    uint8_t  mQuePrvChkSum;
    bool     mQueHadPrv;
    uint16_t mQuePrevID;
    uint16_t mQueSndrId;
    uint8_t  mQueNumSndrs;
    sender_t * mQueSenders;
    uint8_t  * mQueRecByts;
    msgRec_t * mQueRecStats;
    uint8_t  * mQuePrvMsgByts;
    spin_lock_t       * mQueQLock;
    volatile bitQue_t * mQueBitQueP;
} msgQue_t;

/*-----------------------------------------------------------------*/
extern void putNxtMsg( volatile msgQue_t * msgQ );
extern void freeLastMsg( volatile msgQue_t * msgQ );
extern bool tryMsgBuf( volatile msgQue_t * msgQ, uint32_t * tStampP );

/*-----------------------------------------------------------------*/
extern void putNxtWrd( volatile bitQue_t * bitQ, uint32_t nxtWrd );
extern uint32_t getNxtWrd( volatile bitQue_t * bitQ );
extern bool tryWrdBuf( volatile bitQue_t * bitQ );
extern void poll_FIFO_callback( void * bitQp);
extern bool tryBitBuf( volatile bitQue_t * bitQ );
extern bool getNxtBit_isSet( volatile bitQue_t * bitQ );

extern void msgChkPrevAndQue( bool chkPass, uint8_t sndrIdx, uint8_t chkSumRxd, volatile uint8_t * msgP, int len,
                       volatile msgQue_t * msgQ, volatile msgRec_t * msgRecP );

#define QUEUES_FOR_MSGS_AND_BITS_H
#endif
