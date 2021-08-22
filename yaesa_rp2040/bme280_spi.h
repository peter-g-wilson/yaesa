#ifndef BME280_SPI_H

#define BME280_SNDR_ID 0x000F

extern void BME280_init(void);
extern int BME280_read(uint32_t tStamp);

#define BME280_SPI_H
#endif
