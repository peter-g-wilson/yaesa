#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"

#include "wh1080_decode_bits.h"
#include "f007t_decode_bits.h"
#include "queues_for_msgs_and_bits.h"
#include "output_format.h"
#include "serial_io.h"
#include "proj_board.h"
#include "bme280_spi.h"
#include "ds18b20_1w.h"
#include "led_control.h"
#include "sched_ms.h"
#include "f007t_tx_relay.h"

enum ledctl_colours_t sndrToColour(int sndr) {
    typedef struct sndrColrEle_struct {
        uint8_t s;
        enum ledctl_colours_t  c;
    } sndrColrEle_t;

    const sndrColrEle_t sndrColrTbl[] = {
        {  0x0, ledctl_colGreenBright }, // F007
        {  0x1, ledctl_colGreenDim    }, // F007
        {  0x2, ledctl_colCyan        }, // F007
        {  0x3, ledctl_colYellow      }, // F007
        {  0x4, ledctl_colMagenta     }, // F007
        {  0x5, ledctl_colBlueBright  }, // F007 TX relay WH1080 temp, humidity, wind, rain
        {  0x6, ledctl_colRedBright   }, // F007 TX relay DS18B20 temp
        {  0xA, ledctl_colBlueBright  }, // WH1080 temp, humidity, wind, rain
        {  0xB, ledctl_colBlueDim     }, // WH1080 date/time
        {  0xE, ledctl_colRedBright   }, // DS18B20 temp
        {  0xF, ledctl_colRedDim      }, // BME280 temp, humidiy, pressure
    };
    if ((sndr >= 0) && (sndr <= 0xF)) {
        for (uint i = 0; i < sizeof(sndrColrTbl)/sizeof(sndrColrEle_t); i++) {
            if (sndrColrTbl[i].s == sndr) return sndrColrTbl[i].c;
        }
    }
    return ledctl_colTableSize;
}

#define UARTRXID  1
uint8_t uartioRxBuf[256];
uint    uartioRxLen = 0;
#define STDIORXID 2
uint8_t stdioRxBuf[256];
uint    stdioRxLen = 0;

int procSerialRx( uint8_t id, uint8_t * ipBuff, int ipLen ) {
    int rdyNxt = 0;
    if (*ipBuff == '1') {
        rdyNxt = ledctl_kbip_eval( ipBuff+1, ipLen-1);
    }
    if (*ipBuff == '2') {
        rdyNxt = ledctl_kbip_cycl( ipBuff+1, ipLen-1);
    }
    if (!rdyNxt) {
        if (id == UARTRXID)
            uartioRxLen = ipLen;
        else
            stdioRxLen = ipLen;
    }
    return rdyNxt;
}

bool core1_init_done = false;

void core1_entry() {

    puts("Hello, other world!");

    uartIO_init(UARTRXID, procSerialRx,uartioRxBuf,sizeof(uartioRxBuf));
    uartIO_rxEnable( true );

    stdioRx_init(STDIORXID, procSerialRx,stdioRxBuf,sizeof(stdioRxBuf));
    stdioRx_enable( true );

    BME280_init();
    DS18B20_init();
    F007T_tx_relay_init();
    sched_init_core();
    core1_init_done = true;

#define PACING_DELAY_MS    1000
#define BME280_PERIOD_MS  60000
#define DS18B20_PERIOD_MS 60000
#define PERIOD_STAGGER    30000
    uint32_t tNow = to_ms_since_boot( get_absolute_time() );
    uint32_t BME280_tPrev  = tNow;
    uint32_t DS18B20_tPrev = tNow - PERIOD_STAGGER;

    while (true) {
        bool done_delay = false;
        int sndr;
        bool gotSomeWH1080, gotSomeF007T;
        do {
            uint32_t tStampWH1080, tStampF007T;
            gotSomeWH1080 = WH1080_tryMsgBuf(&tStampWH1080);
            gotSomeF007T  = F007T_tryMsgBuf(&tStampF007T);
            bool doWH1080 = false;
            bool doF007T  = false;
            if (gotSomeWH1080 && !gotSomeF007T) {
                doWH1080 = true;
            } else if (!gotSomeWH1080 && gotSomeF007T) {
                doF007T = true;
            } else if (gotSomeWH1080 && gotSomeF007T) {
                uint32_t tDiff = tStampWH1080 - tStampF007T;
                // "relatively" small tDiff likely means F007T is older than WH1080
                if (tDiff <= (uint32_t)0x7FFFFFFF) {
                    doF007T = true;
                } else {
                    doWH1080 = true;
                }
            }
            if (doWH1080) {
                sndr = WH1080_doMsgBuf();
                ledctl_put( sndrToColour(sndr) );
                sleep_ms(PACING_DELAY_MS);
                done_delay = true;
                ledctl_put( ledctl_colAllOff );
            } else if (doF007T) {
                sndr = F007T_doMsgBuf();
                ledctl_put( sndrToColour(sndr) );
                sleep_ms(PACING_DELAY_MS);
                done_delay = true;
                ledctl_put( ledctl_colAllOff );
            }
        } while (gotSomeWH1080 || gotSomeF007T);

        tNow = to_ms_since_boot( get_absolute_time() );
        if ((tNow - BME280_tPrev) > BME280_PERIOD_MS) {
            BME280_tPrev = tNow;
            ledctl_put( sndrToColour(BME280_SNDR_ID) );
            BME280_read(tNow);
            sleep_ms(PACING_DELAY_MS);
            done_delay = true;
            ledctl_put( ledctl_colAllOff );
        }
        tNow = to_ms_since_boot( get_absolute_time() );
        if ((tNow - DS18B20_tPrev) > DS18B20_PERIOD_MS) {
            DS18B20_tPrev = tNow;
            ledctl_put( sndrToColour(DS18B20_SNDR_ID) );
            if (DS18B20_read(tNow) < 0) sleep_ms(PACING_DELAY_MS);
            done_delay = true;
            ledctl_put( ledctl_colAllOff );
        }
        if (uartioRxLen > 0) {
            printf("%*.*s",uartioRxLen,uartioRxLen,&uartioRxBuf[0]);
            uartioRxLen = 0;
            uartIO_rxEnable(true);
        }
        if (stdioRxLen > 0) {
            if (*stdioRxBuf == '0') {   
                sched_printStats();
            }
            else {
                printf("%*.*s",stdioRxLen,stdioRxLen,&stdioRxBuf[0]);
            }
            stdioRxLen = 0;
            stdioRx_enable(true);
        }
        if (!done_delay) sleep_ms(PACING_DELAY_MS);
    }
}
int main() {
    stdio_init_all();

    ledctl_init();

    ledctl_put( ledctl_colWhiteBright );
    puts("Hello, world!");

    WH1080_init( 200, 100 );
    F007T_init(  100,  50 );
    sched_init_core();

    multicore_launch_core1(core1_entry);

    while (!core1_init_done)
        tight_loop_contents();

    WH1080_enable();
    F007T_enable();
    ledctl_put( ledctl_colAllOff );

    while (1)
        tight_loop_contents();

    F007T_uninit();
    WH1080_uninit();
}
