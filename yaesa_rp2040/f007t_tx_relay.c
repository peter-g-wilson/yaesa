#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "pico/binary_info.h"

#include "f007t_tx_manch.pio.h"
#include "f007t_tx_relay.h"
#include "f007t_decode_bits.h"
#include "string.h"
#include "proj_board.h"

uint8_t calc_tx_chksum( uint8_t *buffptr, uint8_t buff_strt, uint8_t buff_size ) {
    // https://eclecticmusingsofachaoticmind.wordpress.com/2015/01/21/home-automation-temperature-sensors/
    uint8_t chksum = 0x64;
    for (uint8_t bytcnt = 0; bytcnt < buff_size; bytcnt++) {
        uint8_t bytval = buffptr[bytcnt+buff_strt];
        for (uint8_t bitcnt = 0; bitcnt < 8; bitcnt++) {
            if (bytval & 0x80) {
                chksum ^= F007TlsfrMask[bytcnt*8+bitcnt]; 
            }
            bytval = bytval << 1;
        }
    }
    return chksum;
}
#define PAYLOAD_STRT 2
#define PAYLOAD_SIZE 5
#define PAYLOAD_RR (PAYLOAD_STRT + 1)
#define PAYLOAD_xT (PAYLOAD_STRT + 2)
#define PAYLOAD_TT (PAYLOAD_STRT + 3)
#define PAYLOAD_CC (PAYLOAD_STRT + PAYLOAD_SIZE)
/*-----------------------------------------------------------------*/
void F007T_tx_relay(uint8_t rfId, float tempDegC, bool batLow) {
    //                                     |<-------PAYLOAD_SIZE----->|
    static uint8_t txBuff[] = {0xFF, 0xFD, 0x46, 0xBA, 0x24, 0x92, 0x0A, 0x27};
    // static uint8_t txBuff[] = {0xC0, 0xC0, 0x82, 0x82, 0x24, 0x92, 0x0A, 0x27};
    uint32_t fifoBuf1 = 0;
    uint32_t fifoBuf2 = 0;
    if (tempDegC < -40.0F) tempDegC = -40.0F;
    uint16_t f007Raw = (uint16_t)((tempDegC * 10.0F + 400.0F)*9.0F/5.0F);
    txBuff[PAYLOAD_RR] = 0x80 | rfId;
    txBuff[PAYLOAD_xT] = (batLow << 7) | (rfId << 4) | ((uint8_t)(f007Raw >> 8));
    txBuff[PAYLOAD_TT] = (uint8_t) f007Raw;
    txBuff[PAYLOAD_CC] = calc_tx_chksum(txBuff, (uint8_t)PAYLOAD_STRT, (uint8_t)PAYLOAD_SIZE);
    fifoBuf1 = (txBuff[0] << 24) | (txBuff[1] << 16) | (txBuff[2] << 8) | txBuff[3] ;
    fifoBuf2 = (txBuff[4] << 24) | (txBuff[5] << 16) | (txBuff[6] << 8) | txBuff[7];
    pio_sm_put_blocking( F007T_TX_PIO_HW, F007T_TX_SM_OP, 64 * 3 - 1);
    pio_sm_put_blocking( F007T_TX_PIO_HW, F007T_TX_SM_OP, fifoBuf1);
    pio_sm_put_blocking( F007T_TX_PIO_HW, F007T_TX_SM_OP, fifoBuf2);
    pio_sm_put_blocking( F007T_TX_PIO_HW, F007T_TX_SM_OP, fifoBuf1);
    pio_sm_put_blocking( F007T_TX_PIO_HW, F007T_TX_SM_OP, fifoBuf2);
    pio_sm_put_blocking( F007T_TX_PIO_HW, F007T_TX_SM_OP, fifoBuf1);
    pio_sm_put_blocking( F007T_TX_PIO_HW, F007T_TX_SM_OP, fifoBuf2);
}
/*-----------------------------------------------------------------*/
void F007T_tx_relay_init(void) {
    pio_sm_set_pins_with_mask(F007T_TX_PIO_HW, F007T_TX_SM_OP, 0, 1u << F007T_TX_PIN);
    pio_sm_set_consecutive_pindirs(F007T_TX_PIO_HW, F007T_TX_SM_OP, F007T_TX_PIN, 1, true);
    pio_gpio_init(F007T_TX_PIO_HW, F007T_TX_PIN);

    uint offset_prog = pio_add_program(F007T_TX_PIO_HW, &F007T_tx_program);
    pio_sm_config cfg_prog = F007T_tx_program_get_default_config(offset_prog);

    sm_config_set_sideset_pins(   &cfg_prog, F007T_TX_PIN);
    sm_config_set_out_shift(      &cfg_prog, false, true, 32);
    sm_config_set_fifo_join(      &cfg_prog, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv_int_frac(&cfg_prog, 10172, 135);

    pio_sm_init(F007T_TX_PIO_HW, F007T_TX_SM_OP, offset_prog+F007T_tx_offset_start_block, &cfg_prog);
    pio_sm_clear_fifos(  F007T_TX_PIO_HW, F007T_TX_SM_OP);
    pio_sm_set_enabled(  F007T_TX_PIO_HW, F007T_TX_SM_OP, true);

    bi_decl(bi_1pin_with_name(F007T_TX_PIN, F007T_TX_CONFIG));
}
void F007T_tx_relay_uninit(void) {
    pio_sm_set_enabled(F007T_TX_PIO_HW, F007T_TX_SM_OP, false);
}
