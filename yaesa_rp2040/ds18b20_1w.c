
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/binary_info.h"

#include "queues_for_msgs_and_bits.h"
#include "output_format.h"
#include "serial_io.h"
#include "proj_board.h"
#include "ds18b20_1w.h"
#include "ds18b20_1w.pio.h"

#define PULL_TXFIFO_STALLED ((uint32_t)(1u << (PIO_FDEBUG_TXSTALL_LSB + DS18B20_SM_1W)))

uint offset_prog_rd_start;
uint offset_prog_rst_start;

void wait_until_blocked_on_txfifo(void)
{
    DS18B20_PIO_HW->fdebug = PULL_TXFIFO_STALLED;
    while ((DS18B20_PIO_HW->fdebug & PULL_TXFIFO_STALLED) != PULL_TXFIFO_STALLED) {
        sleep_us(5);
    }
}
void check_blocked_on_txfifo(uint8_t * msg)
{
    if  ((DS18B20_PIO_HW->fdebug & PULL_TXFIFO_STALLED) != PULL_TXFIFO_STALLED) {
        printf("%s entered when FIFOs not blocked %08X\n",msg, DS18B20_PIO_HW->fdebug);
    }
}
void setPinState( bool drvHi ) {
    if (drvHi) {
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_set(pio_pins, 1));
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_set(pio_pindirs, 1));
    } else {
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_set(pio_pindirs, 0));
    }
}
void write_bytes(uint8_t buf[], uint8_t len)
{
    check_blocked_on_txfifo("Write");
    setPinState( true );
    for (uint i = 0; i < len; i++ ) {
        // the pin is already an output driven high, prime the sm by setting x to the loop bit count less 1
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_set(pio_x, 8-1));
        // release the sm blocking PULL with the 8 data bits to be written
        pio_sm_put_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, buf[i]);
        sleep_us(250); // Write 8 bits takes about 500+ us
        wait_until_blocked_on_txfifo();
    }
}

void read_bytes(uint8_t *buf, uint8_t len)
{
    check_blocked_on_txfifo("Read");
    // prime sm with pin floating but ready to be diven low and for each byte set x to the bit loop count less 1
    pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_set(pio_pindirs, 0));
    pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_set(pio_pins, 0));
    for (uint i = 0; i < len; i++) {
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_set(pio_x, 8-1));
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_jmp(offset_prog_rd_start));
        buf[i] = pio_sm_get_blocking(DS18B20_PIO_HW, DS18B20_SM_1W) >> 24;
        wait_until_blocked_on_txfifo();
    }
}

#define DS18B20_ERR_NONE       0
#define DS18B20_ERR_SHORTTOGND 1
#define DS18B20_ERR_NOPRESENCE 2
#define DS18B20_ERR_CRC        3
uint DS18B20_errCnt  = 0;
uint DS18B20_lastErr = DS18B20_ERR_NONE;

#define RST_PIN_MASK 0x01000000

bool reset(void)
{
    uint32_t pinVal;
    bool res = false;
    check_blocked_on_txfifo("Reset");
    // set pin as floating input
    pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_set(pio_pindirs, 0));
    // read the pin and check that it is being pulled high while floating
    for (uint i = 0; i < 100; i++) {
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_in(pio_pins, 1));
        // want just 1 bit so shift in dummy remaining 7 bits to fill the fifo (8 bit threshold)
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_in(pio_null, 7));
        pinVal = pio_sm_get_blocking(DS18B20_PIO_HW, DS18B20_SM_1W);
        res = (pinVal & RST_PIN_MASK) == RST_PIN_MASK;
        if (res) break;
        sleep_us(2);
    }
    if (!res) {
        DS18B20_errCnt++;
        DS18B20_lastErr = DS18B20_ERR_SHORTTOGND;
    } else {
        // prime sm from floating input to output driven high and the x and y loop counts -1
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_set(pio_pins,    1));
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_set(pio_pindirs, 1));
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_set(pio_x, 6-1));
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_set(pio_y, 5-1));
        // the sm is blocked waiting on PULL, the injected jmp goes to the reset sequence
        // which delays with the pin set low, then floated and read, and then continues the delay
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_jmp(offset_prog_rst_start));
        sleep_us(800); // after the pulse presence and part way through the 480+ us float high 
        wait_until_blocked_on_txfifo();
        // want just 1 bit so shift in dummy remaining 7 bits to fill the fifo (8 bit threshold)
        pio_sm_exec_wait_blocking(DS18B20_PIO_HW, DS18B20_SM_1W, pio_encode_in(pio_null, 7));
        pinVal = pio_sm_get_blocking(DS18B20_PIO_HW, DS18B20_SM_1W);
        // the pin should have been low during the presence pulse that was driven by the DS18B20
        res = (pinVal & RST_PIN_MASK) == 0;
        if (!res) {
            DS18B20_errCnt++;
            DS18B20_lastErr = DS18B20_ERR_NOPRESENCE;
        }
    }
    return res;
}

uint8_t crc8_calc(const uint8_t *addr, uint8_t len)
{
    uint8_t crc = 0;
    while (len--) {
        uint8_t bytVal = *addr++;
        for (uint i = 0; i < 8; i++) {
            uint bitVal = (crc ^ bytVal) & 1;
            crc >>= 1;
            if (bitVal) crc ^= 0x8C;
            bytVal >>= 1;
        }
    }
    return crc;
}

#define DS18B20_MAXMSGBYTS 9

bool ds18b20_read_raw(uint8_t *buff)
{
    // assumes only one DS18B20 on the bus and default 12 bit resolution
    uint8_t hasPwr = 0;
    uint8_t res = false;
    if (reset()) {
        write_bytes((uint8_t[]){0xCC, 0xB4}, 2); // skip ROM then read if power supplied
        read_bytes(&hasPwr,1);
        if (reset()) {
            write_bytes((uint8_t[]){0xCC, 0x44}, 2); // skip ROM then start convert
            setPinState( !hasPwr );
            sleep_ms( 760 ); // Conv_Time_12_bit max 750 ms
            if (reset()) {
                write_bytes((uint8_t[]){0xCC, 0xBE}, 2); // skip ROM then read scratchpad
                read_bytes(buff, DS18B20_MAXMSGBYTS);
                res = true;
            }
        }
    }
    setPinState( false );
    if (res) {
        uint8_t crc = crc8_calc(buff, DS18B20_MAXMSGBYTS);
        res = crc == 0;
        if (!res) {
            DS18B20_errCnt++;
            DS18B20_lastErr = DS18B20_ERR_CRC;
        }
    }
    return res;
}

void DS18B20_init(void)
{
    pio_gpio_init(DS18B20_PIO_HW, DS18B20_PIN);
    gpio_set_pulls(DS18B20_PIN, false, false);

    uint offset_prog_1w       = pio_add_program(DS18B20_PIO_HW, &DS18B20_1w_program);
    uint offset_wr_stall      = offset_prog_1w + DS18B20_1w_offset_wr_stall;
    offset_prog_rd_start      = offset_prog_1w + DS18B20_1w_offset_rd_bits;
    offset_prog_rst_start     = offset_prog_1w + DS18B20_1w_offset_rst_start;
    pio_sm_config cfg_prog_1w = DS18B20_1w_program_get_default_config(offset_prog_1w);
    sm_config_set_in_pins(   &cfg_prog_1w, DS18B20_PIN);
    sm_config_set_set_pins(  &cfg_prog_1w, DS18B20_PIN, 1);
    sm_config_set_out_pins(  &cfg_prog_1w, DS18B20_PIN, 1);
    sm_config_set_in_shift(  &cfg_prog_1w, true, true, 8);
    sm_config_set_out_shift( &cfg_prog_1w, true, true, 8);
    sm_config_set_clkdiv(    &cfg_prog_1w, 312.5F);

    pio_sm_init(       DS18B20_PIO_HW, DS18B20_SM_1W, offset_wr_stall, &cfg_prog_1w);
    pio_sm_set_enabled(DS18B20_PIO_HW, DS18B20_SM_1W, true);

    bi_decl(bi_1pin_with_name(DS18B20_PIN, DS18B20_CONFIG));
}

outBuff_t DS18B20msg;

#define DS18B20_MAXMSGFRMT 15
#define DS18B20_PREDASHPAD  (OFRMT_HEXCODS_LEN - DS18B20_MAXMSGBYTS * 2 + 1)
#define DS18B20_POSTDASHPAD (OFRMT_DECODES_LEN - DS18B20_MAXMSGFRMT)

int DS18B20_read(uint32_t tStamp)
{
    static uint32_t prevStamp = 0;
    uint8_t rx_buff[DS18B20_MAXMSGBYTS];
    uint msgId = 0x000E;
    bool read_res = ds18b20_read_raw(&rx_buff[0]);
    if (read_res) {
        int16_t temperature = (int16_t)((rx_buff[1] << 8) | rx_buff[0]);
        float msgDlta = (float)((uint32_t)(tStamp - prevStamp)) * 0.001F;
        prevStamp = tStamp;

        float temp_degC = temperature * 0.0625F;
        if (temp_degC < -99.0F) temp_degC = -99.0F;
        if (temp_degC > 999.0F) temp_degC = 999.0F;

        int msgLen = opfrmt_snprintf_header( DS18B20msg, msgId, tStamp, rx_buff,
                                             DS18B20_MAXMSGBYTS, DS18B20_PREDASHPAD );
        msgLen += snprintf(&DS18B20msg[msgLen], OFRMT_TOTAL_LEN - msgLen - DS18B20_POSTDASHPAD + 1,
                           "i:%01X,b:0,T:%05.1f", msgId, temp_degC);
        msgLen += opfrmt_snprintf_dashpad( DS18B20msg, msgLen, DS18B20_POSTDASHPAD );
        msgLen += 3;
        OPFRMT_PRINT_PATHETIC_EXCUSE(msgId, msgLen);

        DS18B20msg[msgLen - 3] = 13;
        DS18B20msg[msgLen - 2] = 10;
        DS18B20msg[msgLen - 1] = 0;

        uartIO_buffSend(&DS18B20msg[0], msgLen-1);
        printf("%08X %*.*s Msg %1X %07.1f s Err %d Last %d\n", 
               tStamp, msgLen - 3, msgLen - 3, (uint8_t *)&DS18B20msg[0],
               msgId, msgDlta, DS18B20_errCnt, DS18B20_lastErr);
    }
    if (read_res) return DS18B20_SNDR_ID;
            else return -1;
}
