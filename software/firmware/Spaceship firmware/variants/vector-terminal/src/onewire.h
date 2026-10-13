#ifndef _ONEWIRE_H
#define _ONEWIRE_H
#include <stdint.h>
#include "main.h"
#define OW_Bit 0
extern uint8_t ow_power;
uint8_t ds_start_conversion(void);
uint8_t ds_read_scrach(unsigned char *buff);
uint8_t ds_calc_crc8(void);
int16_t ds_convert_temp(void);
#endif
