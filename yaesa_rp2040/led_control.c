#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "pico/binary_info.h"

#include "proj_board.h"
#include "led_control.h"

#ifdef LED_PWM_RGB

#define LEDCTL_PWM_WRAP 0xFFFF

uint pwm_rg_slice_num;
uint pwm_b_slice_num;
volatile bool ledctl_override = false;

typedef struct ledctl_colours_struct {
    uint16_t r,g,b; 
} ledctl_colours_t;

ledctl_colours_t ledctl_colours[ledctl_colTableSize] = {
    {      0,      0,      0 }, // all off
    {      0,      0, 0x1f00 }, // blue dim
    {      0,      0, 0x8000 }, // blue
    {      0,      0, 0xFFFF }, // blue bright
    {      0, 0x0400,      0 }, // green dim
    {      0, 0x4000,      0 }, // green
    {      0, 0xFFFF,      0 }, // green bright
    {      0, 0x5000, 0x8000 }, // cyan
    { 0x1500,      0,      0 }, // red dim
    { 0x8000,      0,      0 }, // red
    { 0xFFFF,      0,      0 }, // red bright
    { 0x3000,      0, 0x5000 }, // magenta
    { 0xF000, 0x3000, 0x0000 }, // yellow
    { 0x3333, 0x4000, 0x4000 }, // white
    { 0xFFFF, 0xFFFF, 0xFFFF }, // white bright
};

#endif

void ledctl_put(enum ledctl_colours_t colour)
{
#ifdef LED_PWM_RGB
    if (!ledctl_override && (colour < ledctl_colTableSize)) {
        pwm_set_chan_level( pwm_b_slice_num, PWM_CHAN_A, ledctl_colours[colour].b);
        pwm_set_both_levels(pwm_rg_slice_num, ledctl_colours[colour].r, ledctl_colours[colour].g);
    }
#else
    if (colour == ledctl_colAllOff)
        gpio_put(LED_DEFAULT_PIN, LED_DEFAULT_OFF);
    else
        gpio_put(LED_DEFAULT_PIN, LED_DEFAULT_ON);
#endif
}

int ledctl_kbip_cycl( uint8_t * buf, uint len )
{
    int rdyNxt = 0;

#ifdef LED_PWM_RGB
    static int  i = 0;
    static bool drtnUp = false;
    bool drtnPress = false;
    bool prevOvrd  = ledctl_override;
    switch (*buf) {
        case 'S': ledctl_override = !ledctl_override; break;
        case 'U': drtnUp = true;  drtnPress = true;   break;
        case 'D': drtnUp = false; drtnPress = true;   break;
        default:                                      break;
    }
    if (drtnPress) {
        if (drtnUp) { if (++i >= ledctl_colTableSize) i = 0; }
        else        { if (--i < 0) i = ledctl_colTableSize - 1; }
    }
    if (ledctl_override) {
        if (ledctl_override != prevOvrd) printf("LED cycle ON\n");
        uint16_t r = ledctl_colours[i].r;
        uint16_t g = ledctl_colours[i].g;
        uint16_t b = ledctl_colours[i].b;
        pwm_set_chan_level( pwm_b_slice_num, PWM_CHAN_A, b);
        pwm_set_both_levels(pwm_rg_slice_num, r, g);
        printf("i=%02d: r=0x%04X g=0x%04X b=0x%04X\n", i, r,g,b);
    } else {
        pwm_set_chan_level( pwm_b_slice_num, PWM_CHAN_A, 0);
        pwm_set_both_levels(pwm_rg_slice_num, 0, 0);
        printf("LED cycle OFF\n");
    }
    rdyNxt = 1;

#endif
    return rdyNxt;
}

int ledctl_kbip_eval( uint8_t * buf, uint len )
{
    int rdyNxt = 0;

#ifdef LED_PWM_RGB
    static uint16_t i = 0x1000;
    static uint16_t r = 0x8000;
    static uint16_t g = 0x8000;
    static uint16_t b = 0x8000;
    static uint16_t * colrP = &r;
    static uint8_t    colrC = 'R';
    static bool      drtnUp = false;
    uint8_t prevColrC = colrC;
    bool    prevOvrd  = ledctl_override;
    bool    drtnPress = false;
    bool    colrPress = false;
    uint    nibblMask = i * 0xF;
    switch (*buf) {
        case 'S': ledctl_override = !ledctl_override;        break;
        case 'R': colrP = &r; colrC = 'R'; colrPress = true; break;
        case 'G': colrP = &g; colrC = 'G'; colrPress = true; break;
        case 'B': colrP = &b; colrC = 'B'; colrPress = true; break;
        case 'U': drtnUp = true;           drtnPress = true; break;
        case 'D': drtnUp = false;          drtnPress = true; break;
        case 'E': if (i >= 0x0010) i >>= 4;                  break;
        case 'W': if (i <= 0x0100) i <<= 4;                  break;
        default:                                             break;
    }
    if ((colrPress && (colrC == prevColrC)) || drtnPress) {
        if (drtnUp) { if ((*colrP & nibblMask) != nibblMask) *colrP += i; }
        else        { if ((*colrP & nibblMask) != 0        ) *colrP -= i; }
    }
    if (ledctl_override) {
        if (ledctl_override != prevOvrd) printf("LED eval ON\n");
        pwm_set_both_levels(pwm_rg_slice_num, r, g);
        pwm_set_chan_level(pwm_b_slice_num, PWM_CHAN_A, b);
        printf("r=0x%04X g=0x%04X b=0x%04X: col=%c, dir=%c, step=0x%04X\n", r,g,b,colrC,drtnUp ? 'U':'D',i);
    } else {
        pwm_set_both_levels(pwm_rg_slice_num, 0, 0);
        pwm_set_chan_level(pwm_b_slice_num, PWM_CHAN_A, 0);
        printf("LED eval OFF: col=%c, dir=%c, step=0x%04X\n", colrC, drtnUp ? 'U':'D', i);
    }
    rdyNxt = 1;

#endif
    return rdyNxt;
}

void ledctl_init(void)
{
#ifdef LED_PWM_RGB
    gpio_set_function(LED_PWM_A_RED,   GPIO_FUNC_PWM);
    gpio_set_function(LED_PWM_B_GREEN, GPIO_FUNC_PWM);
    gpio_set_function(LED_PWM_A_BLUE,  GPIO_FUNC_PWM);
    pwm_rg_slice_num = pwm_gpio_to_slice_num(LED_PWM_A_RED);
    pwm_b_slice_num  = pwm_gpio_to_slice_num(LED_PWM_A_BLUE);
    pwm_config pwm_rgb_config = pwm_get_default_config();
    pwm_init(pwm_rg_slice_num, &pwm_rgb_config, false);
    pwm_init(pwm_b_slice_num,  &pwm_rgb_config, false);

    pwm_set_clkdiv_int_frac(pwm_rg_slice_num, 10, 0);
    pwm_set_clkdiv_int_frac(pwm_b_slice_num,  10, 0);

    pwm_set_wrap(pwm_rg_slice_num, LEDCTL_PWM_WRAP);
    pwm_set_wrap(pwm_b_slice_num,  LEDCTL_PWM_WRAP);

    pwm_set_both_levels(pwm_rg_slice_num, 0, 0);
    pwm_set_both_levels(pwm_b_slice_num,  0, 0);

    pwm_set_output_polarity(pwm_rg_slice_num, true, true);
    pwm_set_output_polarity(pwm_b_slice_num,  true, true);

    // pwm_hw->slice[pwm_b_slice_num].ctr = LEDCTL_PWM_WRAP/2;

    pwm_set_mask_enabled((1 << pwm_rg_slice_num) | (1 << pwm_b_slice_num));

    bi_decl(bi_1pin_with_name(LED_RED_PIN, LED_RED_CONFIG));
    bi_decl(bi_1pin_with_name(LED_GREEN_PIN, LED_GRN_CONFIG));
    bi_decl(bi_1pin_with_name(LED_BLUE_PIN, LED_BLU_CONFIG));

#else
    gpio_init(LED_DEFAULT_PIN);
    gpio_set_dir(LED_DEFAULT_PIN, GPIO_OUT);
    gpio_put(LED_DEFAULT_PIN, LED_DEFAULT_OFF);
    bi_decl(bi_1pin_with_name(LED_DEFAULT_PIN, LED_DEFAULT_CONFIG));

#endif
}
