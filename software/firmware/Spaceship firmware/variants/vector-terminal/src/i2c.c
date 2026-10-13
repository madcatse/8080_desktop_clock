/*
 * Copyright (c) 2009, Zilogic Systems <code@zilogic.com>
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "i2c.h"
#include "main.h"

#ifdef EMU
__sfr __at 0x30 pcf1;
__sfr __at 0x31 pcf2;
__sfr __at 0x32 pcf3;
__sfr __at 0x33 pcf4;
__sfr __at 0x34 pcf5;
__sfr __at 0x35 pcf6;
__sfr __at 0x36 pcf7;
#endif

#define SCL_bit	4h		// SCL port PC.4
#define SDA_bit	0h		// SDA port PC.0

uint8_t SDA;
/* Keep the physical packet for diagnostics, before masking unused bits. */
uint8_t rtc_raw[7];
#ifndef EMU
uint8_t rtc_need_start=1;
#endif

void sclset() __naked
{
	#asm
	 ; SCL HI without changing SDA     	
	mvi a,00001001b	; set bit
	out PIO_M
        ret
	#endasm
}

void sclclr() __naked
{
	#asm
	  ; SCL LO without changing SDA       	
	mvi a,00001000b	; clear bit
	out PIO_M
        ret
	#endasm
}

void sdaset() __naked
{
	#asm
	; SDA HI without changing SCL
	mvi a,00000001b	; set bit
	out PIO_M
        ret
	#endasm
}
void sdaclr() __naked
{
	#asm
	 ; SDA LO without changing SCL   	
	mvi a,00000000b	; clear bit
	out PIO_M
        ret
	#endasm
}

// I2C functions
void i2c_init() __naked
{
	#asm
	mvi A,PIO_C_IN
	out PIO_M
	mvi a,00001001b	; set bit
	out PIO_M
	ret
	#endasm
}


/* START condition */
void i2c_start(void)
{
	#asm
	mvi a,PIO_C_OUT
	out PIO_M		
	#endasm
	sdaset();
	sclset();
	sdaclr();
	sclclr();
	sdaset();
	
}

/* Issuing a STOP condition */

void i2c_stop(void)
{
	#asm
	mvi a,PIO_C_OUT
	out PIO_M		
	#endasm
        sdaclr();
        sclset();
        sdaset();

}

/* Clock pulse generation. The function returns data or acknowledgment
   bit */

uint8_t i2c_clock_read(void)
{
	#asm
	call _sclset
	in PIO_C			; SDA (RX data bit) is in A.0
	ani 1
	sta _SDA
	call _sclclr
	#endasm
	return(SDA);
}

void i2c_clock(void)
{
	sclset();
	sclclr();
}

/* Writing a byte to a slave, with most significant bit first. The
   function returns acknowledgment bit. */

uint8_t i2c_write(uchar byte)
{
	uchar mask = 0x80;
	uint8_t t_ret;

	while (mask) {
		if (byte & mask)
			sdaset();
		else
			sdaclr();
		i2c_clock();
		mask >>= 1;
	}
	sdaset();
		#asm
		mvi a,PIO_C_IN       		; SDA - in, SCL - out
		out PIO_M   		;
		#endasm
	t_ret = i2c_clock_read();
		#asm
		mvi a,PIO_C_OUT       		; SDA - out, SCL - out
		out PIO_M   		;
		#endasm
	return (t_ret);
}

/* Reading byte from a slave, with most significant bit
   first. The parameter indicates, whether to acknowledge
   (1) or not (0) */

uchar i2c_read(uint8_t acknowledgment)
{
	uchar mask = 0x80, byte = 0x00;

		#asm
		mvi a,PIO_C_IN       		; SDA - in, SCL - out
		out PIO_M   		;
		#endasm

	while (mask) {
		if (i2c_clock_read())
			byte |= mask;
		mask >>= 1;	/* next bit to receive */
	}

		#asm
		mvi a,PIO_C_OUT       		; SDA - out, SCL - out
		out PIO_M   		;
		#endasm
	if (acknowledgment) {
		sdaclr();
		i2c_clock();
		sdaset();
	} else {
		sdaset();
		i2c_clock();
	}
	return (byte);
}


uint8_t i2c_read_regs(uint8_t slave, uint8_t reg, uint8_t *buf, uint8_t n) {
    uint8_t ok=0;
    i2c_start();
    if (i2c_write(slave) || i2c_write(reg)) goto done;
    i2c_start();
    if (i2c_write(slave|1)) goto done;
    while (n) { --n; *buf++=i2c_read(n!=0); }
    ok=1;
done:
    i2c_stop();return ok;
}
uint8_t i2c_write_regs(uint8_t slave, uint8_t reg, const uint8_t *buf, uint8_t n) {
    uint8_t ok=0;
    i2c_start();
    if (i2c_write(slave) || i2c_write(reg)) goto done;
    while (n--) if (i2c_write(*buf++)) goto done;
    ok=1;
done:
    i2c_stop();return ok;
}
uint8_t rtc_write(uint8_t addr, uint8_t value) { return i2c_write_regs(PCF8563,addr,&value,1); }
uint8_t rtc_init(void) {
#ifndef EMU
    rtc_need_start=!rtc_write(0x00,0x00);
    return !rtc_need_start && rtc_write(0x0d,0x00);
#else
    return 1;
#endif
}
uint8_t rtc_read_time(uint8_t r[]) {
#ifndef EMU
    uint8_t tries;
    if (rtc_need_start) {
        if (!rtc_write(0,0)) return 0;
        rtc_need_start=0;
    }
    /* Each failed transaction ends with STOP; retry a transient NACK once. */
    for (tries=0;tries<2;tries++) if (i2c_read_regs(PCF8563,2,rtc_raw,7)) break;
    if (tries==2) return 0;
#else
    rtc_raw[0]=pcf1;rtc_raw[1]=pcf2;rtc_raw[2]=pcf3;rtc_raw[3]=pcf4;
    rtc_raw[4]=pcf5;rtc_raw[5]=pcf6;rtc_raw[6]=pcf7;
#endif
    for (uint8_t i=0;i<7;i++) r[i]=rtc_raw[i];
    /* PCF8563P unused bits are not BCD digits. Retain VL and century. */
    r[1]&=0x7f;r[2]&=0x3f;r[3]&=0x3f;r[4]&=7;r[5]&=0x9f;
    return 1;
}
uint8_t rtc_write_time(const uint8_t *r) {
#ifndef EMU
    uint8_t ok;
    if (!rtc_write(0x00,0x20)) return 0;
    rtc_need_start=1;
    ok=i2c_write_regs(PCF8563,2,r,7);
    /* Restart even after a failed write. Both transactions always STOP. */
    if (!rtc_write(0x00,0)) ok=0;else rtc_need_start=0;
    return ok;
#else
    pcf1=r[0];pcf2=r[1];pcf3=r[2];pcf4=r[3];pcf5=r[4];pcf6=r[5];pcf7=r[6];
    return 1;
#endif
}
