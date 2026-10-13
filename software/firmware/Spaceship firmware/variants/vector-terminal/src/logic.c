#include "logic.h"
#include <stdio.h>
uint32_t total_seconds, last_stamp;
uint8_t have_stamp, clock_jump;
uint8_t bcd_dec(uint8_t v) { return (v>>4)*10+(v&15); }
uint8_t dec_bcd(uint8_t v) { return v/10*16+v%10; }
uint8_t month_days(uint8_t m, uint8_t y) {
    if (m==2) return y%4==0 ? 29 : 28; /* 2000..2099, including 2000 */
    return (m==4 || m==6 || m==9 || m==11) ? 30 : 31;
}
uint8_t calendar_valid(uint8_t *r) {
    uint8_t i, v;
    for (i=0;i<7;i++) {
        v=r[i];
        if (i==0) v &= 0x7f;
        if (i==5) v &= 0x1f;
        if ((v&15)>9 || (v>>4)>9) return 0;
    }
    return (r[0]&0x7f)<0x60 && r[1]<0x60 && r[2]<0x24
        && r[4]<7 && (r[5]&0x1f)>=1 && (r[5]&0x1f)<=0x12
        && r[3]>=1 && bcd_dec(r[3])<=month_days(bcd_dec(r[5]&0x1f),bcd_dec(r[6]));
}
uint8_t edit_index[6]={2,1,4,3,5,6};
uint8_t edit_limit[7]={59,59,23,31,6,12,99};
void calendar_edit(uint8_t *r, uint8_t field, uint8_t plus) {
    uint8_t i,v,lo,hi;
    if (!field || field>6) return;
    i=edit_index[field-1];
    lo=(i==3 || i==5)?1:0;
    hi=edit_limit[i];
    if (i==3) hi=month_days(bcd_dec(r[5]&0x1f),bcd_dec(r[6]));
    v=i==4?r[i]:bcd_dec(r[i]&(i==5?0x1f:0xff));
    v=plus?(v>=hi?lo:v+1):(v<=lo?hi:v-1);
    r[i]=i==4?v:dec_bcd(v);
    hi=month_days(bcd_dec(r[5]&0x1f),bcd_dec(r[6]));
    if (bcd_dec(r[3])>hi) r[3]=dec_bcd(hi);
    r[0] &= 0x7f; /* deliberate correction acknowledges VL */
}
uint32_t calendar_stamp(uint8_t *r) {
    uint8_t y,m,i;
    uint16_t days;
    y=bcd_dec(r[6]);m=bcd_dec(r[5]&0x1f);
    days=(uint16_t)y*365+(y+3)/4;
    for (i=1;i<m;i++) days+=month_days(i,y);
    days+=bcd_dec(r[3])-1;
    return (uint32_t)days*86400L+(uint32_t)bcd_dec(r[2])*3600L+(uint16_t)bcd_dec(r[1])*60+bcd_dec(r[0]&0x7f);
}
void clock_rebase(void) { have_stamp=0; }
/* A constant long % small integer is narrowed to 16 bits by sccz80 2.4.
   Keep the divisor a run-time parameter and subtract the full quotient. */
uint16_t clock_mod(uint32_t value, uint16_t period) {
    return value-(value/(uint32_t)period)*(uint32_t)period;
}
uint16_t clock_observe(uint8_t *r) {
    uint32_t now,d;
    clock_jump=0;
    now=calendar_stamp(r);
    if (!have_stamp) { have_stamp=1;last_stamp=now;clock_jump=1;return 0; }
    if (now>=last_stamp) d=now-last_stamp;
    else if (last_stamp>3155759900UL && now<120UL) d=3155760000UL-last_stamp+now;
    else d=0xffffffffUL;
    last_stamp=now;
    if (d>120) { clock_jump=1;return 0; }
    total_seconds+=d;
    return d;
}
int16_t ds_decode(uint8_t *p) {
    int16_t raw;
    uint16_t mag;
    raw=(int16_t)((uint16_t)p[1]*256+p[0]);
    /* Undefined low bits for 9/10/11-bit resolutions. */
    raw &= (int16_t)(0xffff << (3-((p[4]>>5)&3)));
    mag=raw<0?(uint16_t)-raw:(uint16_t)raw;
    mag=(mag*10+8)/16;
    return raw<0?-(int16_t)mag:(int16_t)mag;
}
void format_temp(char *out, int16_t t10) {
    uint16_t v=t10<0?(uint16_t)-t10:(uint16_t)t10;
    sprintf(out,"%c%u.%u",t10<0?'-':'+',v/10,v%10);
}
uint16_t displayed_humidity(uint32_t h) { return h*10/1024; }
uint16_t displayed_pressure(uint32_t p) { return p*75/10000; }
