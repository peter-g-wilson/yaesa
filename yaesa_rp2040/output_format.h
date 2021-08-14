
#ifndef  OUTPUT_FORMAT_H

//                        1         2            1         2         3         4
// 1234-12345678-1234567890123456789012-1234567890123456789012345678901234567890123456789
#define OFRMT_SNDR_ID_LEN  4
#define OFRMT_TSTAMP_LEN   8
#define OFRMT_HEXCODS_LEN 22
#define OFRMT_DECODES_LEN 49
#define OFRMT_SNDR_TSTAMP_LEN (OFRMT_SNDR_ID_LEN + 1 + OFRMT_TSTAMP_LEN + 1)
#define OFRMT_TOTAL_LEN (OFRMT_SNDR_TSTAMP_LEN + OFRMT_HEXCODS_LEN + 1 + OFRMT_DECODES_LEN)
#define OUTBUFF_TOTAL_LEN (OFRMT_TOTAL_LEN + 2 + 1)

#define OFRMT_PRINT_PATHETIC_EXCUSE( id, len ) \
    if (len != sizeof(outBuff_t))\
        printf("There might have already been a TRAP so its possibly too late for this message!\n"\
               "ID %04X: Expected %d but got %d - printf formats are tricky stuff\n",id,sizeof(outBuff_t),len)

typedef uint8_t outBuff_t[OUTBUFF_TOTAL_LEN];
extern const uint8_t dash_padding[OFRMT_TOTAL_LEN - OFRMT_SNDR_TSTAMP_LEN + 1];

typedef struct outArgsStruct {
    int      oArgMsgLen;
    uint8_t  oArgSndrId;
    uint     oArgPrvChkInvld;
    uint32_t oArgMsgVrfdDltaTim;
    uint32_t oArgMsgSmplTimStamp;
    uint32_t oArgMsgSmplTimDlta;
    uint     oArgMsgSmplVrfyCnt;
    uint     oArgMsgSmplUnVrfyCnt;
    uint     oArgMsgSmplHdrHitsCnt;
    uint     oArgMsgSmplChkErrsCnt;
    uint32_t oArgBitSmplTimDlta;
    uint     oArgBitSmplTotCnt;
    uint     oArgBitSmplOnesCnt;
    uint     oArgMsgHiWtr;
    uint     oArgWrdHiWtr;
    uint     oArgFiFoHiWtr;
} outArgs_t;

extern void print_msg( uint8_t outBuff[OUTBUFF_TOTAL_LEN], outArgs_t * outArgsP );
extern void output_copy_args( int len, uint8_t id, volatile msgQue_t * msgQ, volatile msgRec_t * msgRecP, outArgs_t * outArgsP);

#define OUTPUT_FORMAT_H
#endif
