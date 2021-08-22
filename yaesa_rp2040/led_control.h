#ifndef LED_CONTROL_H

enum ledctl_colours_t {
    ledctl_colAllOff = 0,
    ledctl_colBlueDim,
    ledctl_colBlue,
    ledctl_colBlueBright,
    ledctl_colGreenDim,
    ledctl_colGreen,
    ledctl_colGreenBright,
    ledctl_colCyan,
    ledctl_colRedDim,
    ledctl_colRed,
    ledctl_colRedBright,
    ledctl_colMagenta,  
    ledctl_colYellow,
    ledctl_colWhite,
    ledctl_colWhiteBright,
    ledctl_colTableSize
};

extern void ledctl_init( void );
extern void ledctl_put( enum ledctl_colours_t colour );
extern int  ledctl_kbip_eval( uint8_t * buf, uint len );
extern int  ledctl_kbip_cycl( uint8_t * buf, uint len );

#define LED_CONTROL_H
#endif