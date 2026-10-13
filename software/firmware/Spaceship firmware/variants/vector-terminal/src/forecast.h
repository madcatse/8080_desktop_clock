#ifndef _FORECAST_H
#define _FORECAST_H

#include <stdint.h>

void setH(int h);
uint8_t addP(int32_t p, int16_t t10);
int32_t seaPressure(int32_t p, int16_t t10);
void setMonth(uint8_t month);
uint8_t getCast(void);
int16_t getTrend(void);
void forecast_reset(void);
#endif
