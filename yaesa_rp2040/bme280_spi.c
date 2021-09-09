/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"

/* Example code to talk to a bme280 humidity/temperature/pressure sensor.

   NOTE: Ensure the device is capable of being driven at 3.3v NOT 5v. The Pico
   GPIO (and therefor SPI) cannot be used at 5v.

   You will need to use a level shifter on the SPI lines if you want to run the
   board at 5v.

   Connections on Raspberry Pi Pico board and a generic bme280 board, other
   boards may vary.

   GPIO 16 (pin 21) MISO/spi0_rx-> SDO/SDO on bme280 board
   GPIO 17 (pin 22) Chip select -> CSB/!CS on bme280 board
   GPIO 18 (pin 24) SCK/spi0_sclk -> SCL/SCK on bme280 board
   GPIO 19 (pin 25) MOSI/spi0_tx -> SDA/SDI on bme280 board
   3.3v (pin 36) -> VCC on bme280 board
   GND (pin 38)  -> GND on bme280 board

   Note: SPI devices can have a number of different naming schemes for pins. See
   the Wikipedia page at https://en.wikipedia.org/wiki/Serial_Peripheral_Interface
   for variations.

   This code uses a bunch of register definitions, and some compensation code derived
   from the Bosch datasheet which can be found here.
   https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf
*/

/*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
/* A quick and dirty fix to re-use example code from Raspberry Pi (Trading) Ltd */
#undef  spi_default
#undef  PICO_DEFAULT_SPI_RX_PIN
#undef  PICO_DEFAULT_SPI_CSN_PIN
#undef  PICO_DEFAULT_SPI_SCK_PIN
#undef  PICO_DEFAULT_SPI_TX_PIN
/* velleman VMA335 specific naming scheme
   SPI_HW      spi instance (spi0 or spi1 with GPIOs to match)
   SPI_RX_PIN  MISO SPI_HW_rx  -> SDO
   SPI_CS_PIN  CS   SPI_HW_csn -> CSB
   SPI_CLK_PIN SCK  SPI_HW_sck -> SCL
   SPI_TX_PIN  MOSI SPI_HW_tx  -> SDA
*/
#include "proj_board.h"
#define spi_default              SPI_HW
#define PICO_DEFAULT_SPI_RX_PIN  SPI_RX_PIN
#define PICO_DEFAULT_SPI_CSN_PIN SPI_CS_PIN
#define PICO_DEFAULT_SPI_SCK_PIN SPI_CLK_PIN
#define PICO_DEFAULT_SPI_TX_PIN  SPI_TX_PIN
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "bme280_spi.h"
#include "queues_for_msgs_and_bits.h"
#include "output_format.h"
#include "serial_io.h"
/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

#define READ_BIT 0x80

int32_t t_fine;

uint16_t dig_T1;
int16_t dig_T2, dig_T3;
uint16_t dig_P1;
int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
uint8_t dig_H1, dig_H3;
int8_t dig_H6;
int16_t dig_H2, dig_H4, dig_H5;

/* The following compensation functions are required to convert from the raw ADC
data from the chip to something usable. Each chip has a different set of
compensation parameters stored on the chip at point of manufacture, which are
read from the chip at startup and used inthese routines.
*/
int32_t compensate_temp(int32_t adc_T) {
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t) dig_T1 << 1))) * ((int32_t) dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t) dig_T1)) * ((adc_T >> 4) - ((int32_t) dig_T1))) >> 12) * ((int32_t) dig_T3))
            >> 14;

    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T;
}

uint32_t compensate_pressure(int32_t adc_P) {
    int32_t var1, var2;
    uint32_t p;
    var1 = (((int32_t) t_fine) >> 1) - (int32_t) 64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t) dig_P6);
    var2 = var2 + ((var1 * ((int32_t) dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t) dig_P4) << 16);
    var1 = (((dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) + ((((int32_t) dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t) dig_P1)) >> 15);
    if (var1 == 0)
        return 0;

    p = (((uint32_t) (((int32_t) 1048576) - adc_P) - (var2 >> 12))) * 3125;
    if (p < 0x80000000)
        p = (p << 1) / ((uint32_t) var1);
    else
        p = (p / (uint32_t) var1) * 2;

    var1 = (((int32_t) dig_P9) * ((int32_t) (((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((int32_t) (p >> 2)) * ((int32_t) dig_P8)) >> 13;
    p = (uint32_t) ((int32_t) p + ((var1 + var2 + dig_P7) >> 4));

    return p;
}

uint32_t compensate_humidity(int32_t adc_H) {
    int32_t v_x1_u32r;
    v_x1_u32r = (t_fine - ((int32_t) 76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t) dig_H4) << 20) - (((int32_t) dig_H5) * v_x1_u32r)) +
                   ((int32_t) 16384)) >> 15) * (((((((v_x1_u32r * ((int32_t) dig_H6)) >> 10) * (((v_x1_u32r *
                                                                                                  ((int32_t) dig_H3))
            >> 11) + ((int32_t) 32768))) >> 10) + ((int32_t) 2097152)) *
                                                 ((int32_t) dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t) dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);

    return (uint32_t) (v_x1_u32r >> 12);
}

#ifdef PICO_DEFAULT_SPI_CSN_PIN
static inline void cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0);  // Active low
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
    asm volatile("nop \n nop \n nop");
}
#endif

#if defined(spi_default) && defined(PICO_DEFAULT_SPI_CSN_PIN)
static void write_register(uint8_t reg, uint8_t data) {
    uint8_t buf[2];
    buf[0] = reg & 0x7f;  // remove read bit as this is a write
    buf[1] = data;
    cs_select();
    spi_write_blocking(spi_default, buf, 2);
    cs_deselect();
    sleep_ms(10);
}

static void read_registers(uint8_t reg, uint8_t *buf, uint16_t len) {
    // For this particular device, we send the device the register we want to read
    // first, then subsequently read from the device. The register is auto incrementing
    // so we don't need to keep sending the register we want, just the first.
    reg |= READ_BIT;
    cs_select();
    spi_write_blocking(spi_default, &reg, 1);
    sleep_ms(10);
    spi_read_blocking(spi_default, 0, buf, len);
    cs_deselect();
    sleep_ms(10);
}

/* This function reads the manufacturing assigned compensation parameters from the device */
void read_compensation_parameters() {
    uint8_t buffer[26];

    read_registers(0x88, buffer, 24);

    dig_T1 = buffer[0] | (buffer[1] << 8);
    dig_T2 = buffer[2] | (buffer[3] << 8);
    dig_T3 = buffer[4] | (buffer[5] << 8);

    dig_P1 = buffer[6] | (buffer[7] << 8);
    dig_P2 = buffer[8] | (buffer[9] << 8);
    dig_P3 = buffer[10] | (buffer[11] << 8);
    dig_P4 = buffer[12] | (buffer[13] << 8);
    dig_P5 = buffer[14] | (buffer[15] << 8);
    dig_P6 = buffer[16] | (buffer[17] << 8);
    dig_P7 = buffer[18] | (buffer[19] << 8);
    dig_P8 = buffer[20] | (buffer[21] << 8);
    dig_P9 = buffer[22] | (buffer[23] << 8);

    dig_H1 = buffer[25];

    read_registers(0xE1, buffer, 8);

    dig_H2 = buffer[0] | (buffer[1] << 8);
    dig_H3 = (int8_t) buffer[2];
    dig_H4 = buffer[3] << 4 | (buffer[4] & 0xf);
    dig_H5 = (buffer[5] >> 4) | (buffer[6] << 4);
    dig_H6 = (int8_t) buffer[7];
}

uint BME280_errCnt = 0;

bool BME280_checkId( void )
{
    static bool checkedOk = false;
    // See if SPI is working - interrograte the device for its I2C ID number, should be 0x60
    uint8_t id;
    read_registers(0xD0, &id, 1);
    if (id != 0x60) {
        checkedOk = false;
        BME280_errCnt++;
        printf("Chip ID error, got 0x%02x\n", id);
    } else {
        if (!checkedOk) {
            checkedOk = true;

            read_compensation_parameters();
    
            write_register(0xF2, 0x1); // Humidity oversampling register - going for x1
            write_register(0xF4, 0x27);// Set rest of oversampling modes and run mode to normal
        }
    }
    return checkedOk;
}

#endif

void BME280_init()
{
#if !defined(spi_default) || !defined(PICO_DEFAULT_SPI_SCK_PIN) || !defined(PICO_DEFAULT_SPI_TX_PIN) || !defined(PICO_DEFAULT_SPI_RX_PIN) || !defined(PICO_DEFAULT_SPI_CSN_PIN)
#warning spi/bme280_spi example requires a board with SPI pins
    puts("Default SPI pins were not defined");
#else

    printf("Hello, bme280! Reading raw data from registers via SPI...\n");

    // This example will use SPI0 at 0.5MHz.
    spi_init(spi_default, 500 * 1000);
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    // Make the SPI pins available to picotool
    bi_decl(bi_3pins_with_func(PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI));

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
    // Make the CS pin available to picotool
    bi_decl(bi_1pin_with_name(PICO_DEFAULT_SPI_CSN_PIN, "SPI CS"));

    BME280_checkId();
    
#endif
}

outBuff_t BME280msg;

#define BME280_MAXMSGBYTS 8
#define BME280_HEXCODES_LEN (OFRMT_HEADER_LEN + BME280_MAXMSGBYTS * 2)
#define BME280_MAXMSGFRMT 30
#define BME280_PREDASHPAD  (OFRMT_HEXCODS_LEN - BME280_MAXMSGBYTS * 2)
#define BME280_POSTDASHPAD (OFRMT_DECODES_LEN - BME280_MAXMSGFRMT)

int BME280_read(uint32_t tStamp)
{
#if !defined(spi_default) || !defined(PICO_DEFAULT_SPI_SCK_PIN) || !defined(PICO_DEFAULT_SPI_TX_PIN) || !defined(PICO_DEFAULT_SPI_RX_PIN) || !defined(PICO_DEFAULT_SPI_CSN_PIN)
#warning spi/bme280_spi example requires a board with SPI pins
    puts("Default SPI pins were not defined");
    return -1;
#else
    int32_t humidity, pressure, temperature;
    uint8_t buffer[BME280_MAXMSGBYTS];
    static uint32_t prevStamp = 0;
    float  msgDlta = (float)((uint32_t)(tStamp - prevStamp))*0.001F;
    prevStamp = tStamp;

    bool read_res = BME280_checkId();

    if (read_res) {
        read_registers(0xF7, buffer, 8);
        pressure = ((uint32_t) buffer[0] << 12) | ((uint32_t) buffer[1] << 4) | (buffer[2] >> 4);
        temperature = ((uint32_t) buffer[3] << 12) | ((uint32_t) buffer[4] << 4) | (buffer[5] >> 4);
        humidity = (uint32_t) buffer[6] << 8 | buffer[7];
    
        // These are the raw numbers from the chip, so we need to run through the
        // compensations to get human understandable numbers
        float press_hPa = compensate_pressure(pressure)/100.0F;
        float temp_degC = compensate_temp(temperature)/100.0F;
        int   humid_pct = compensate_humidity(humidity)/1024;
        if (press_hPa < 0.0F   ) press_hPa = 0.0F;
        if (press_hPa > 9999.0F) press_hPa = 9999.0F;
        if (temp_degC < -99.0F)  temp_degC = -99.0F;
        if (temp_degC > 999.0F)  temp_degC = 999.0F;
        if (humid_pct < 0  )     humid_pct = 0;
        if (humid_pct > 999)     humid_pct = 999;
    
        uint msgId = BME280_SNDR_ID;
        int msgLen = snprintf( &BME280msg[0], OFRMT_HEADER_LEN+1, "%03d %0*X-%0*X-",
                                 OpMsgSeqNum, OFRMT_SNDR_ID_LEN, msgId, OFRMT_TSTAMP_LEN, tStamp);
        OpMsgSeqNum++;
        for (uint i = 0; i < BME280_MAXMSGBYTS; i++) {
            msgLen += snprintf( &BME280msg[OFRMT_HEADER_LEN+(i*2)], 2+1, "%02X", buffer[i] );
        }
        msgLen += snprintf( &BME280msg[BME280_HEXCODES_LEN], OFRMT_TOTAL_LEN - BME280_HEXCODES_LEN + 1,
                   "%*.*s-i:%01X,b:0,T:%05.1f,H:%03d,P:%06.1f%*.*s",
                   BME280_PREDASHPAD, BME280_PREDASHPAD, dash_padding,
                   msgId, temp_degC, humid_pct, press_hPa,
                   BME280_POSTDASHPAD, BME280_POSTDASHPAD, dash_padding);
    
        msgLen += 3;
        OFRMT_PRINT_PATHETIC_EXCUSE(msgId,msgLen);
    
        BME280msg[msgLen-3] = 13;
        BME280msg[msgLen-2] = 10;
        BME280msg[msgLen-1] = 0;
    
        uartIO_buffSend(&BME280msg[0],msgLen);
        printf("%08X %*.*s Msg %1X %07.1f s Err %d\n",
               tStamp,msgLen-3,msgLen-3,(uint8_t *)&BME280msg[0],
               msgId,msgDlta,BME280_errCnt);
    }
    if (read_res) return BME280_SNDR_ID;
            else return -1;
#endif
}
