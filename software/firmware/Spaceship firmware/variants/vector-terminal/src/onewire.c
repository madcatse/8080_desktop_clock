#include "main.h"
#include "onewire.h"
#include "logic.h"

extern unsigned char scrachpad[9];
uint8_t ow_power;

uint8_t ow_init(void) __naked
{
	#asm
	mvi a,PIO_B_IN
	out PIO_M

	mvi a,PIO_B_OUT
	out PIO_M

	mvi a,OW_Bit	; clear bit
	out PIO_B

	mvi b,47*3
w_lp1:	dcr b
	jnz w_lp1

	mvi a,PIO_B_IN
	out PIO_M

	mvi b,15
w_lp2:	dcr b
	jnz w_lp2

	in PIO_B
	ani 1
	xri 1
	mov l,a
	mvi h,0

	mvi b,104
w_lp6:	dcr b
	jnz w_lp6
	ret
	#endasm
}

void ow_write(unsigned char data) __z88dk_fastcall __naked
{
	#asm
	push d
	mov c,l

        mvi      b,8

ow_pb1:  

	mov a,c
	rrc                    ;B[7] => CY
	mov c,a

	jnc ow_put_low
	mvi a,PIO_B_IN
	out PIO_M
	mvi a,PIO_B_OUT	; SDA + SCL output
	out PIO_M	;
	mvi a,OW_Bit	; clear bit
	out PIO_B
	mvi a,PIO_B_IN
	out PIO_M

	jmp ow_pnxt
ow_put_low:
	mvi a,PIO_B_IN
	out PIO_M
	mvi a,PIO_B_OUT	; SDA + SCL output
	out PIO_M	;
	mvi a,OW_Bit	; clear bit
	out PIO_B

	mvi d,2	; zero slot: measured 109..115 us including 8255 switching
w_lp4:	dcr d
	jnz w_lp4
ow_pnxt:
	mvi d,18
w_lp3:	dcr d
	jnz w_lp3


	dcr b
        jnz    ow_pb1

	lda _ow_power
	ora a
	jz ow_release
	mvi a,PIO_B_OUT
	out PIO_M
	mvi a,1
	out PIO_B
	jmp ow_write_done
ow_release:
	mvi a,PIO_B_IN
	out PIO_M
ow_write_done:
	pop d
	ret
	#endasm
}

unsigned char ow_read(void) __naked
{
	#asm
	push d
		mvi b,8
ow_gb1:    
	mvi a,PIO_B_OUT       		
	out PIO_M   		;

	mvi a,PIO_B_IN
	out PIO_M

;	nop
;	nop
;	nop
;	nop

	in PIO_B			; SDA (RX data bit) is in A.0

	mvi d,18
w_lp5:	dcr d
	jnz w_lp5

	rrc					; move RX data bit to CY
	mov a,c
	rar                    	; Shift CY into C
	mov c,a
	dcr b
        jnz    ow_gb1
        mov l,c             		; Return RX Byte in L
	mvi h,0
	pop d
	ret
	#endasm
}

/* Dallas reflected CRC-8. RRC already inserts old bit 0 into bit 7,
   so the conditional XOR is 0x0c rather than 0x8c. HL is fastcall. */
uint8_t ds_crc8(uint8_t *pad) __z88dk_fastcall __naked {
 #asm
 push d
 mvi d,8
 xra a
 crc_byte:
 xra m
 inx h
 mvi b,8
 crc_bit:
 rrc
 jnc crc_next
 xri 0ch
 crc_next:
 dcr b
 jnz crc_bit
 dcr d
 jnz crc_byte
 mov l,a
 mvi h,0
 pop d
 ret
 #endasm
}
unsigned char ds_calc_crc8(void) { return ds_crc8(scrachpad); }
int16_t ds_convert_temp(void) { return ds_decode(scrachpad); }
void ow_release_power(void) __naked {
    #asm
    mvi a,PIO_B_IN
    out PIO_M
    ret
    #endasm
}
uint8_t ds_start_conversion(void) {
    ow_power=0;
    if (!ow_init()) return 0;
    ow_write(0xcc);
    ow_power=1; /* PB0 high throughout conversion; no 8255 mode changes allowed. */
    ow_write(0x44);
    return 1;
}
uint8_t ds_read_scrach(unsigned char *buff) {
    uint8_t n;int16_t raw;
    ow_release_power();
    ow_power=0;
    if (!ow_init()) return 0;
    ow_write(0xcc);ow_write(0xbe);
    for (n=0;n<9;n++) buff[n]=ow_read();
    raw=(int16_t)((uint16_t)buff[1]*256+buff[0]);
    return raw>=-880 && raw<=2000 && ds_crc8(buff)==buff[8] && buff[5]==0xff && buff[7]==0x10
        && (buff[4]&0x9f)==0x1f;
}
