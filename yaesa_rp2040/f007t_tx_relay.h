#ifndef F007T_TX_RELAY_H

#define MAX_F007T_KNWNSNDRIDX 4
#define F007T_TX_RFID_WH1080  (MAX_F007T_KNWNSNDRIDX + 1)
#define F007T_TX_RFID_DS18B20 (MAX_F007T_KNWNSNDRIDX + 2)
extern void F007T_tx_relay( uint8_t id, float tempDegC, bool batLow);
extern void F007T_tx_relay_init(void);
extern void F007T_tx_relay_uninit(void);

#define F007T_TX_RELAY_H
#endif
