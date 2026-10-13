/* English instrument UI, independent of the animation's local viewport.
 * No full-screen redraw after startup: digits update changed segments,
 * text updates changed characters, telemetry updates one region per call.
 * Missing/invalid readings display placeholders, never synthetic weather.
 */
#include <stdio.h>
#include <string.h>
#include "lcd.h"
#include "gui.h"
#include "logic.h"
#include "forecast.h"
#include "vector.h"
extern uint8_t timeb[7],settings_mode,clock_valid,bme_ok,ds_ok,weather_ok,app_running;
extern int16_t curr_temp;
extern int32_t temperature;
extern uint32_t pressure,humidity;
extern int16_t bme_temp10(void);
/* Cache slots 0..10: date, weekday, RTC status, temperature, source,
   humidity, pressure, forecast, trend, model state, settings/key hint.
   field() compares characters only; its caller must invalidate/force if
   it needs to recolor otherwise identical text. Current fields use fixed
   colors or change their text when the corresponding state changes. */
static char cache[12][32];
static uint8_t old_digit[6],old_setting=255;
static uint8_t seen[7],clock_bytes[3],date_bytes[4],clock_state,date_state,date_setting;
static char weekdays[7][4]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
static uint8_t temp_source,rh_state,pres_state,wx_state,wx_cast,mode_bytes[3];
static int16_t wx_trend;
static int32_t temp_raw;
static uint32_t rh_raw,pres_raw;
/* Segment bits: top, upper right, lower right, bottom, lower left,
   upper left, middle. Index 10 is a dash for invalid time. */
static uint8_t masks[11]={63,6,91,79,102,109,125,7,127,111,64};
static uint16_t digit_x[6]={20,80,178,238,338,368};
#define INK 0xbfff
#define DIM 0x1144
#define SET_INK 0xfdc0

static void text(uint16_t x,uint16_t y,char *s,uint16_t color) {
 POINT_COLOR=color;BACK_COLOR=BLACK;LCD_ShowString(x,y,s,0);
}
/* Character diffs make a second tick or sensor change a small update.
   Every cache slot is bounded to 31 characters plus NUL. */
static void field(uint8_t slot,uint16_t x,uint16_t y,char *s,uint16_t color) {
 uint8_t i,len,old;
 len=strlen(s);old=strlen(cache[slot]);
 if(len>31)len=31;
 for(i=0;i<len || i<old;i++) {
  /* Trailing spaces erase an old longer value without clearing its box. */
  if(i<len && i<old && s[i]==cache[slot][i]) continue;
  LCD_ShowChar(x+8*i,y,color,BLACK,i<len?s[i]:' ',0);
 }
 memcpy(cache[slot],s,len);cache[slot][len]=0;
}
/* Segment rectangles are sparse strokes, never a large digit rectangle. */
static void digit(uint8_t n,uint8_t value,uint16_t color,uint8_t force) {
 uint8_t b,m,old,t,w,h;uint16_t x,y,sx,sy,ex,ey;
 x=digit_x[n];y=n<4?46:78;w=n<4?46:22;h=n<4?72:36;t=n<4?4:3;
 m=masks[value];old=old_digit[n]==255?255:masks[old_digit[n]];
 /* Settings transitions force recoloring, even if a digit is unchanged. */
 for(b=0;b<7;b++) {
  if(!force && old_digit[n]!=255 && !((m^old)&(1<<b)))continue;
  if(b==0 || b==3 || b==6) {
   sx=x+t;ex=x+w-t;sy=y+(b==0?0:b==3?h-t:h/2-t/2);ey=sy+t-1;
  } else {
   sx=x+((b==1 || b==2)?w-t:0);ex=sx+t-1;
   sy=y+((b==1 || b==5)?t:h/2+t/2);ey=sy+h/2-t-1;
  }
  LCD_Fill(sx,sy,ex,ey,m&(1<<b)?color:0x0841);
 }
 old_digit[n]=value;
}
void terminal_init(void) {
 /* The only full-screen clear. Keep all labels and brackets outside
    X=218..461,Y=165..275, which belongs exclusively to vector_delta. */
 uint8_t i;
 LCD_Clear(BLACK);memset(cache,0,sizeof(cache));memset(old_digit,255,6);memset(seen,0,7);
 text(20,7,"ORBITAL / VECTOR TERMINAL",0x07ff);
 text(356,7,"8080 / LIVE",0x7c93);
 LCD_Fill(18,28,461,28,DIM);
 text(20,29,"LOCAL TIME",0x5bd0);text(338,55,"SECONDS",0x5bd0);
 LCD_Fill(151,63,156,68,INK);LCD_Fill(151,94,156,99,INK);
 LCD_Fill(18,146,194,146,DIM);LCD_Fill(207,146,461,146,DIM);
 text(20,151,"ENVIRONMENT",0x07ff);
 text(20,170,"TEMP",0x5bd0);text(20,207,"RH",0x5bd0);text(20,234,"PRES",0x5bd0);
 text(218,147,"TRACK / PERSPECTIVE",0x5bd0);
 /* Brackets stay outside the animation viewport and are never erased. */
 for(i=0;i<2;i++) {
  LCD_Fill(i?464:214,162,i?468:218,162,0x328a);
  LCD_Fill(i?468:214,162,i?468:214,168,0x328a);
  LCD_Fill(i?464:214,277,i?468:218,277,0x328a);
  LCD_Fill(i?468:214,271,i?468:214,277,0x328a);
 }
 LCD_Fill(18,295,461,295,DIM);
 text(20,300,"1 MODEL   2 PAUSE   3 SET TIME",0x7c93);
 vector_init();
}
/* Only one UI region per main loop, so telemetry never batches a full
   screen worth of writes ahead of keys or a vector step. */
void terminal_render(void) {
 /* Round-robin phases: clock, calendar/status, temperature, humidity,
    pressure, forecast/trend, model/settings. Raw-state caches avoid
    sprintf and LCD traffic when a region's inputs have not changed. */
 static uint8_t phase;
 uint8_t i,v,force,source,cast;uint16_t color;int16_t trend;int32_t raw;char s[32];
 if(phase==0) {
  if(seen[0] && clock_state==clock_valid && old_setting==settings_mode && !memcmp(clock_bytes,timeb,3))goto done;
  memcpy(clock_bytes,timeb,3);clock_state=clock_valid;seen[0]=1;
  force=old_setting!=settings_mode;old_setting=settings_mode;
  for(i=0;i<6;i++) {
   v=i<2?timeb[2]:i<4?timeb[1]:timeb[0]&0x7f;
   v=i&1?v&15:v>>4;
   if(!clock_valid && !settings_mode)v=10;
   color=INK;
   if(settings_mode && settings_mode==(i<2?1:i<4?2:0))color=SET_INK;
   digit(i,v,color,force);
  }
 } else if(phase==1) {
  if(seen[1] && date_state==clock_valid && date_setting==settings_mode && !memcmp(date_bytes,timeb+3,4))goto done;
  memcpy(date_bytes,timeb+3,4);date_state=clock_valid;date_setting=settings_mode;seen[1]=1;
  if(clock_valid || settings_mode)sprintf(s,"%02u.%02u.20%02u",bcd_dec(timeb[3]),bcd_dec(timeb[5]&31),bcd_dec(timeb[6]));
  else strcpy(s,"--.--.----");
  field(0,20,123,s,INK);
  /* sccz80 2.4 mishandles a logical-OR ternary's condition flags.
     Explicit branches also keep the untrusted-time placeholder clear. */
  if(clock_valid || settings_mode)strcpy(s,weekdays[timeb[4]<7?timeb[4]:0]);
  else strcpy(s,"---");
  field(1,136,123,s,0x7c93);
  field(2,218,123,clock_valid?"RTC LOCK":"RTC NO RELIABLE TIME",clock_valid?0x07ff:SET_INK);
 } else if(phase==2) {
  source=ds_ok?1:bme_ok?2:0;raw=ds_ok?(int32_t)curr_temp:temperature;
  if(seen[2] && source==temp_source && raw==temp_raw)goto done;
  temp_source=source;temp_raw=raw;seen[2]=1;
  if(ds_ok || bme_ok) {format_temp(s,ds_ok?curr_temp:bme_temp10());strcat(s," C");}else strcpy(s,"-- C");
  field(3,68,170,s,INK);
  field(4,68,187,ds_ok?"DS18B20":bme_ok?"BME280":"NO SENSOR",0x5bd0);
 } else if(phase==3) {
  if(seen[3] && rh_state==bme_ok && rh_raw==humidity)goto done;
  rh_state=bme_ok;rh_raw=humidity;seen[3]=1;
  if(bme_ok) {v=displayed_humidity(humidity)/10;sprintf(s,"%u.%u %%",v,displayed_humidity(humidity)%10);}else strcpy(s,"-- %");
  field(5,68,207,s,INK);
 } else if(phase==4) {
  if(seen[4] && pres_state==bme_ok && pres_raw==pressure)goto done;
  pres_state=bme_ok;pres_raw=pressure;seen[4]=1;
  if(bme_ok)sprintf(s,"%u mmHg",displayed_pressure(pressure));else strcpy(s,"-- mmHg");
  field(6,68,234,s,INK);
 } else if(phase==5) {
  cast=weather_ok?getCast():0;trend=weather_ok?getTrend():0;
  if(seen[5] && wx_state==weather_ok && wx_cast==cast && wx_trend==trend)goto done;
  wx_state=weather_ok;wx_cast=cast;wx_trend=trend;seen[5]=1;
  /* Forecast is the inherited pressure/temperature trend, never demo data. */
  if(weather_ok) {
   v=cast;strcpy(s,v<2?"CLEAR":v<4?"CLOUDY":v<6?"SHOWERS":v<8?"RAIN":"STORM");
  } else strcpy(s,"WX NO DATA");
  field(7,20,253,s,weather_ok?0x07ff:0x7c93);
  if(weather_ok)sprintf(s,"TREND %+d Pa",getTrend());else strcpy(s,"BME / DS TELEMETRY");
  field(8,20,271,s,0x5bd0);
 } else {
  if(seen[6] && mode_bytes[0]==model && mode_bytes[1]==paused && mode_bytes[2]==settings_mode)goto done;
  mode_bytes[0]=model;mode_bytes[1]=paused;mode_bytes[2]=settings_mode;seen[6]=1;
  sprintf(s,"%s / %s",model?"SHIP":"CUBE",paused?"HOLD":"ROTATE");field(9,218,279,s,0x07ff);
  if(settings_mode) {
   sprintf(s,"SET %s  1+ 2- 3NEXT",settings_mode==1?"HOUR":settings_mode==2?"MIN":settings_mode==3?"WDAY":settings_mode==4?"DATE":settings_mode==5?"MONTH":"YEAR");
  } else strcpy(s,"1 MODEL   2 PAUSE   3 SET TIME");
  field(10,20,300,s,settings_mode?SET_INK:0x7c93);
 }
 done:phase=phase==6?0:phase+1;
}
