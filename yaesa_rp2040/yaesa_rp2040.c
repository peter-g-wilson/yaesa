#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/multicore.h"

#include "wh1080_decode_bits.h"
#include "f007t_decode_bits.h"
#include "queues_for_msgs_and_bits.h"
#include "output_format.h"
#include "uart_io.h"
#include "proj_board.h"
#include "bme280_spi.h"
#include "ds18b20_1w.h"

uint8_t rxIpBuff[256];
uint charsRxInput = 0;
void rxIpProcess( int ipLen ) {
    charsRxInput = ipLen;
}

void core1_entry() {
    puts("Hello, other world!");
    uartIO_init((uartIO_rxCallBack_t *)rxIpProcess,rxIpBuff,sizeof(rxIpBuff));
    uartIO_rxEnable( true );

    BME280_init();
    DS18B20_init();

#define PACING_DELAY_MS    1000
#define BME280_PERIOD_MS  60000
#define DS18B20_PERIOD_MS 60000
#define PERIOD_STAGGER    30000
    uint32_t tNow = to_ms_since_boot( get_absolute_time() );
    uint32_t BME280_tPrev  = tNow;
    uint32_t DS18B20_tPrev = tNow - PERIOD_STAGGER;

    while (true) {
        bool done_delay = false;
        if (WH1080_tryMsgBuf()) {
            gpio_put(LED_RED_PIN, LED_DEFAULT_ON);
            WH1080_doMsgBuf();
            sleep_ms(PACING_DELAY_MS);
            done_delay = true;
            gpio_put(LED_RED_PIN, LED_DEFAULT_OFF);
        };
        if (F007T_tryMsgBuf()) {
            gpio_put(LED_BLUE_PIN, LED_DEFAULT_ON);
            F007T_doMsgBuf();
            sleep_ms(PACING_DELAY_MS);
            done_delay = true;
            gpio_put(LED_BLUE_PIN, LED_DEFAULT_OFF);
        };
        tNow = to_ms_since_boot( get_absolute_time() );
        if ((tNow - BME280_tPrev) > BME280_PERIOD_MS) {
            BME280_tPrev = tNow;
            gpio_put(LED_GREEN_PIN, LED_DEFAULT_ON);
            BME280_read(tNow);
            sleep_ms(PACING_DELAY_MS);
            done_delay = true;
            gpio_put(LED_GREEN_PIN, LED_DEFAULT_OFF);
        }
        tNow = to_ms_since_boot( get_absolute_time() );
        if ((tNow - DS18B20_tPrev) > DS18B20_PERIOD_MS) {
            DS18B20_tPrev = tNow;
            gpio_put(LED_GREEN_PIN, LED_DEFAULT_ON);
            if (!DS18B20_read(tNow)) sleep_ms(PACING_DELAY_MS);
            done_delay = true;
            gpio_put(LED_GREEN_PIN, LED_DEFAULT_OFF);
        }
        if (charsRxInput > 0) {
            printf("%*.*s",charsRxInput,charsRxInput,&rxIpBuff[0]);
            charsRxInput = 0;
            uartIO_rxEnable(true);
        }
        if (!done_delay) sleep_ms(PACING_DELAY_MS);
    }
}
/*-----------------------------------------------------------------*/
int main()
{
    stdio_init_all();

    gpio_init(LED_DEFAULT_PIN);
    gpio_set_dir(LED_DEFAULT_PIN, GPIO_OUT);
    gpio_put(LED_DEFAULT_PIN, LED_DEFAULT_OFF);

#if LED_THREE_COLOUR
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, LED_DEFAULT_OFF);

    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, LED_DEFAULT_OFF);
#endif

    gpio_put(LED_DEFAULT_PIN, LED_DEFAULT_ON);
    puts("Hello, world!");

    WH1080_init( 200, 100 );
    F007T_init(  100,  50 );

    multicore_launch_core1(core1_entry);
    gpio_put(LED_DEFAULT_PIN, LED_DEFAULT_OFF);

    while (1)
        tight_loop_contents();

    F007T_uninit();
    WH1080_uninit();
}
