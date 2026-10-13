/* Application scheduler for the physical 8080 board.
 * No interrupt tick: CPU INT is grounded, so main polls RTC and buttons.
 * Each iteration renders one UI region and advances two animation steps.
 * LCD fills also yield through app_background_poll; do not call app_loop
 * from that callback, which would recursively enter the renderers.
 */
#include <stdlib.h>
#include <string.h>
#include "lcd.h"
#include "gui.h"
#include "i2c.h"
#include "onewire.h"
#include "bme280.h"
#include "forecast.h"
#include "logic.h"
#include "vector.h"
/* PCF8563 packet: sec (VL in bit7), min, hour, day, weekday, month, year.
   All fields are BCD except weekday (0=Sunday); month bit7 is century.
   This initial packet is not trusted time until a valid RTC poll arrives.
   "scrachpad" retains the inherited driver symbol spelling. */
uint8_t scrachpad[9],timeb[7]={0,0,0,1,6,1,0};
/* DS temperature is tenths of C; BME temperature centidegrees,
   BME humidity %RH*1024 and pressure Pa. Validity flags gate the UI. */
int16_t curr_temp;
int32_t temperature;
uint32_t pressure,humidity;
uint8_t settings_mode,app_running,clock_valid,rtc_ok,bme_ok,ds_ok,bme_initialized,ds_ticks;
uint8_t forecast_due=1,weather_ok,weather_dirty=1,forecast_source;
uint32_t sensor_due,btn3_time;
uint16_t main_polls,sensor_poll;
uint8_t background_phase,key_state=14,key_raw=14,key_candidate=14,key_ticks;
uint8_t sensor_phase;
uint8_t key_head,key_tail,key_queue[8];
/* Eight-slot ring with one slot reserved (capacity seven key events).
   Keep events queued while a settings edit must wait for DS power release. */
uint16_t edit_polls;
void terminal_init(void);
void terminal_render(void);
uint8_t readkey(void) __naked {
 /* Active-low PC1..PC3. Reprogramming 8255 while PB0 powers the DS
    would reset port B and interrupt conversion; skip the mode write then. */
 #asm
 lda _ow_power
 ora a
 jnz key_input
 mvi a,PIO_C_IN
 out PIO_M
 key_input:
 in PIO_C
 ani 14
 mov l,a
 mvi h,0
 ret
 #endasm
}
void process_keys(uint8_t pressed) {
 /* pressed is a falling-edge bit mask, not the raw active-low pin value.
    Settings order: hour, minute, weekday, day, month, year, exit. */
 uint8_t edit[7];
 if(pressed&8) {
  settings_mode=settings_mode==6?0:settings_mode+1;
  btn3_time=total_seconds;edit_polls=0;
 }
 if(settings_mode && (pressed&6)) {
  btn3_time=total_seconds;edit_polls=0;
  memcpy(edit,timeb,7);calendar_edit(edit,settings_mode,(pressed&2)!=0);
  clock_rebase();forecast_reset();forecast_due=1;
  if(rtc_write_time(edit)) {memcpy(timeb,edit,7);clock_valid=rtc_ok=1;}
  else {rtc_ok=clock_valid=0;}
 } else if(!settings_mode) {
  if(pressed&2)model^=1;
  if(pressed&4)paused^=1;
 }
}
/* Called only once per delayed main loop: eight stable samples debounce.
   No waits, no repeats from holding, no RTC transaction under DS power. */
void keys_tick(void) {
 uint8_t pressed;
 key_raw=readkey();
 if(key_raw!=key_candidate) {key_candidate=key_raw;key_ticks=1;}
 else if(key_ticks<8)++key_ticks;
 if(key_ticks==8 && key_candidate!=key_state) {
  pressed=key_state&~key_candidate;key_state=key_candidate;
  if(pressed && ((key_head+1)&7)!=key_tail) {key_queue[key_head]=pressed;key_head=(key_head+1)&7;}
 }
 if(settings_mode && ++edit_polls>=3000)settings_mode=0;
}
void sample_keys(void) {
 /* LCD yields capture the pins; debouncing advances on the timed loop. */
 key_raw=readkey();
 while(key_tail!=key_head) {
  uint8_t event=key_queue[key_tail];
  /* Visual controls and selecting a field are safe under PB0 power.
     Only an edit that writes the RTC needs to wait for the 1-Wire bus. */
  if(ow_power && (event&6) && (settings_mode || (event&8)))return;
  key_tail=(key_tail+1)&7;process_keys(event);
 }
}
/* BME compensation is validated to -4000..8500 centidegrees. Round the
   magnitude so negative temperatures near zero keep the correct sign. */
int16_t bme_temp10(void) {
    int16_t t=(int16_t)temperature;
    return t<0 ? -((-t+5)/10) : (t+5)/10;
}
void app_service(void) {
    uint8_t r[7],present,source;
    uint16_t elapsed;
    uint32_t stamp;
    /* Two-wire DS18B20: PB0 supplies conversion power. A mode write to
       the 8255 clears PB, so defer I2C and RTC writes for at least 800ms.
       LCD and keys remain usable; RTC catches up on the next poll. */
    if (ow_power) {
        /* msleep(12) measures >10 ms on the actual 8080 instruction path. */
        msleep(12);
        if (++ds_ticks>=80) {
            ds_ok=ds_read_scrach(scrachpad);
            if (ds_ok) curr_temp=ds_convert_temp();
            else {weather_ok=0;weather_dirty=1;forecast_reset();forecast_due=1;}
        }
        sample_keys();return;
    }
    rtc_ok=rtc_read_time(r);
    if (!rtc_ok) clock_valid=0;
    else if (!clock_valid || !have_stamp || memcmp(timeb,r,7)) {
        if (calendar_valid(r)) {
            memcpy(timeb,r,7);
            clock_valid=(r[0]&0x80)==0;
            if (clock_valid) {
                elapsed=clock_observe(r);
                if (clock_jump) {forecast_reset();forecast_due=1;}
                if (elapsed) {
                    stamp=last_stamp;
                    if (clock_mod(stamp,1800) < elapsed) forecast_due=1;
                }
            } else clock_rebase();
        } else clock_valid=0;
    }
    sample_keys();
    if (settings_mode && total_seconds-btn3_time>=6) settings_mode=0;
    if (!sensor_phase && (total_seconds>=sensor_due || (uint16_t)(main_polls-sensor_poll)>=5000)) {
        sensor_due=total_seconds+20;sensor_poll=main_polls;
        sensor_phase=1;
    }
    /* Keep the inherited compensation/drivers, but schedule their costs
       separately. LCD, keys and geometry run between these bus stages. */
    if (sensor_phase==1) {
        if (!bme_initialized) bme_initialized=BME280_begin(MODE_NORMAL);
        sensor_phase=2;return;
    }
    if (sensor_phase==2) {
        bme_ok=bme_initialized && BME280_readMeasurement(&temperature,&humidity,&pressure);
        if (!bme_ok) bme_initialized=0;
        if (!bme_ok) {weather_ok=0;weather_dirty=1;forecast_reset();forecast_due=1;}
        sensor_phase=3;return;
    }
    if (sensor_phase==3) {
        present=ds_start_conversion();ds_ticks=0;
        if (!present) ds_ok=0;
        sensor_phase=0;return;
    }
    source=ds_ok?1:bme_ok?2:0;
    /* Reset trend history when temperature source changes; do not mix
       DS and BME measurements into one pressure compensation history. */
    if (source!=forecast_source) {
        forecast_source=source;forecast_reset();forecast_due=1;weather_ok=0;weather_dirty=1;
    }
    if (forecast_due && bme_ok && !ow_power) {
        setMonth(bcd_dec(timeb[5]&0x1f));
        weather_ok=addP(pressure,ds_ok?curr_temp:bme_temp10());weather_dirty=1;forecast_due=0;
    }
}
/* At most eight 4-row LCD strips (15360 pixels) between full polls.
   Keys are sampled on every yield. Powered DS steps retain their delay. */
void app_background_poll(void) {
    if (!app_running) return;
    background_phase=(background_phase+1)&7;
    if (ow_power || !background_phase) app_service();
    else sample_keys();
}
void app_loop(void) {
  /* Debounce counts stable polls, not milliseconds: cadence varies with
     LCD and sensor work. Service RTC at least every 16 main iterations,
     plus yields from fills; DS power adds its separately timed delay. */
  msleep(2);keys_tick();
  if(ow_power || !(main_polls&15))app_service();else sample_keys();
  /* Two bounded operations: mask geometry, a clear chunk, or <=4 dirty
     row extents per operation. Service between pairs; no whole-frame wipe. */
  terminal_render();vector_step();vector_step();++main_polls;
}
void main(void) {
 /* Altitude is a fixed 255 m inherited forecast setting, not a measurement.
    LCD startup is followed by a black view, matching delta_init's baseline. */
 i2c_init();rtc_init();setH(255);LCD_Init();app_running=1;
 terminal_init();
 while(1)app_loop();
}
