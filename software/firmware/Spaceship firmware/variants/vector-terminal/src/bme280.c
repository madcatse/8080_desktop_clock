///////////////////////////////////////////////////////////////////////////
////                                                                   ////
////                             BME280_Lib.c                          ////
////                                                                   ////
////                      Driver for CCS C compiler                    ////
////                                                                   ////
//// Driver for Bosch BME280 sensor. This sensor can read temperature, ////
//// humidity and pressure.                                            ////
//// This driver only supports I2C mode, it doesn't support SPI mode.  ////
////                                                                   ////
///////////////////////////////////////////////////////////////////////////
////                                                                   ////
////                     https://simple-circuit.com/                   ////
////                                                                   ////
///////////////////////////////////////////////////////////////////////////


#include <stdint.h>
#include <string.h>
#include "bme280.h"
#include "i2c.h"

#define delay_ms msleep

int32_t adc_T, adc_P, adc_H, t_fine;

    uint8_t  BME280_I2C_ADDRESS = 0xEC;

BME280calib BME280_calib;

uint8_t bme_ready, bme_raw_valid;
uint8_t BME280_Write(uint8_t reg, uint8_t v) { return i2c_write_regs(BME280_I2C_ADDRESS,reg,&v,1); }
uint8_t BME280_begin(uint8_t mode) {
    uint8_t c[26],h[7],v,i;
    bme_ready=bme_raw_valid=0;
    if (!i2c_read_regs(BME280_I2C_ADDRESS,0xd0,&v,1) || v!=0x60) return 0;
    if (!BME280_Write(0xe0,0xb6)) return 0;
    msleep(3);
    for (i=0;i<10;i++) {
        if (!i2c_read_regs(BME280_I2C_ADDRESS,0xf3,&v,1)) return 0;
        if (!(v&1)) break;
        msleep(2);
    }
    if (i==10) return 0;
    if (!i2c_read_regs(BME280_I2C_ADDRESS,0x88,c,26)
        || !i2c_read_regs(BME280_I2C_ADDRESS,0xe1,h,7)) return 0;
    /* The first 12 fields are consecutive 16-bit words on our 8080 ABI.
       Their sensor representation is already little endian, including signs. */
    memcpy(&BME280_calib,c,24);
    BME280_calib.dig_H1=c[25];
    BME280_calib.dig_H2=(uint16_t)h[0]+(uint16_t)h[1]*256;
    BME280_calib.dig_H3=h[2];
    BME280_calib.dig_H4=(uint16_t)h[3]*16+(h[4]&15);
    if (BME280_calib.dig_H4&0x800) BME280_calib.dig_H4|=0xf000;
    BME280_calib.dig_H5=(uint16_t)h[5]*16+(h[4]>>4);
    if (BME280_calib.dig_H5&0x800) BME280_calib.dig_H5|=0xf000;
    BME280_calib.dig_H6=h[6];
    if (!BME280_calib.dig_T1 || BME280_calib.dig_T1==0xffff
        || !BME280_calib.dig_P1 || BME280_calib.dig_P1==0xffff) return 0;
    if (!BME280_Write(0xf2,3) || !BME280_Write(0xf5,0x70)
        || !BME280_Write(0xf4,0x68|mode)) return 0;
    /* First normal/forced frame after reset: x4 T, x2 P, x4 H takes
       up to 25.4 ms. Reading skipped raw immediately would reset forever. */
    /* Measure the 8080 path: msleep(32) gives >28 ms at 3.555555 MHz. */
    msleep(32);
    bme_ready=1;return 1;
}
uint8_t BME280_Update(void) {
    uint8_t r[8];
    bme_raw_valid=0;
    if (!bme_ready || !i2c_read_regs(BME280_I2C_ADDRESS,0xf7,r,8)) return 0;
    adc_P=((int32_t)r[0]*65536L+(uint16_t)r[1]*256+r[2])/16;
    adc_T=((int32_t)r[3]*65536L+(uint16_t)r[4]*256+r[5])/16;
    adc_H=(int32_t)r[6]*256+r[7];
    if (adc_T==0x80000L || adc_P==0x80000L || adc_H==0x8000L
        || adc_T==0xfffffL || adc_P==0xfffffL || adc_H==0xffffL) return 0;
    bme_raw_valid=1;return 1;
}
/* Bosch integer compensation: arithmetic shifts, one coherent raw frame.
   Keep the 21-bit humidity bias in DATA: sccz80's constant-add peephole
   loses carry when adding it to a negative long intermediate. */
int32_t bme_h_bias=2097152L;
uint8_t BME280_readTemperature(int32_t *temp) {
    int32_t a,b,d;
    if (!BME280_Update()) return 0;
    a=(((adc_T>>3)-(int32_t)BME280_calib.dig_T1*2)*BME280_calib.dig_T2)>>11;
    d=(adc_T>>4)-BME280_calib.dig_T1;
    b=(((d*d)>>12)*BME280_calib.dig_T3)>>14;
    t_fine=a+b;
    *temp=(t_fine*5+128L)>>8;return 1;
}
uint8_t BME280_readHumidity(uint32_t *humi) {
    int32_t v,a,b;
    if (!bme_raw_valid) return 0;
    v=t_fine-76800L;
    a=((adc_H*16384L-(int32_t)BME280_calib.dig_H4*1048576L
        -(int32_t)BME280_calib.dig_H5*v)+16384L)>>15;
    b=(((v*BME280_calib.dig_H6)>>10)*(((v*(int32_t)BME280_calib.dig_H3)>>11)+32768L))>>10;
    b=((b+bme_h_bias)*BME280_calib.dig_H2+8192L)>>14;
    v=a*b;
    v-=(((((v>>15)*(v>>15))>>7)*(int32_t)BME280_calib.dig_H1)>>4);
    if(v<0)v=0;
    if(v>419430400L)v=419430400L;
    *humi=(uint32_t)v>>12;return 1;
}
uint8_t BME280_readPressure(uint32_t *pres) {
    int32_t a,b;
    uint32_t p;
    if (!bme_raw_valid) return 0;
    a=(t_fine>>1)-64000L;
    b=(((a>>2)*(a>>2))>>11)*BME280_calib.dig_P6;
    b+=a*BME280_calib.dig_P5*2;
    b=(b>>2)+(int32_t)BME280_calib.dig_P4*65536L;
    a=(((BME280_calib.dig_P3*(((a>>2)*(a>>2))>>13))>>3)
        +(((int32_t)BME280_calib.dig_P2*a)>>1))>>18;
    a=((32768L+a)*(int32_t)BME280_calib.dig_P1)>>15;
    if(a<=0)return 0;
    p=((uint32_t)(1048576L-adc_P)-(b>>12))*3125UL;
    if(p<0x80000000UL)p=(p*2)/(uint32_t)a;
    else p=(p/(uint32_t)a)*2;
    a=((int32_t)BME280_calib.dig_P9*(int32_t)(((p>>3)*(p>>3))>>13))>>12;
    b=((int32_t)(p>>2)*BME280_calib.dig_P8)>>13;
    *pres=(uint32_t)((int32_t)p+((a+b+BME280_calib.dig_P7)>>4));return 1;
}
uint8_t BME280_readMeasurement(int32_t *t, uint32_t *h, uint32_t *p) {
    int32_t nt;uint32_t nh,np;
    if (!BME280_readTemperature(&nt) || !BME280_readHumidity(&nh) || !BME280_readPressure(&np)) return 0;
    if (nt < -4000L || nt > 8500L || nh > 102400L || np < 30000L || np > 110000L) return 0;
    *t=nt;*h=nh;*p=np;return 1;
}
