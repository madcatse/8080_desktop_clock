#ifndef CLOCK_LOGIC_H
#define CLOCK_LOGIC_H
#include <stdint.h>
uint8_t dec_bcd(uint8_t v);
uint8_t bcd_dec(uint8_t v);
uint8_t month_days(uint8_t month, uint8_t year);
uint8_t calendar_valid(uint8_t *rtc);
void calendar_edit(uint8_t *rtc, uint8_t field, uint8_t plus);
uint32_t calendar_stamp(uint8_t *rtc);
uint16_t clock_mod(uint32_t value, uint16_t period);
extern uint32_t total_seconds,last_stamp;
extern uint8_t have_stamp;
extern uint8_t clock_jump;
void clock_rebase(void);
uint16_t clock_observe(uint8_t *rtc);
int16_t ds_decode(uint8_t *pad);
void format_temp(char *out, int16_t t10);
uint16_t displayed_humidity(uint32_t h);
uint16_t displayed_pressure(uint32_t p);
#endif
