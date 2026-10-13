/* 
 * File: tftst.h      
 * Author: funStackLabs
 * Comments: ST7735 1.8" SPI TFT screens PIC C driver
 * 
 */

#include "cust_fnt.h"
#include "lcd.h"
#include "math.h"
#include "string.h"
#include "gui.h"

uint16_t __fgColor = 0;
uint16_t __bgColor = 0;
uint16_t __blend[16];

void tfstPrepareBlend(uint16_t color1, uint16_t color2) {
    if (__fgColor == color1 && __bgColor == color2) {
        return;
    }
/*
    uint8_t color1Red = color1 >> 11;
    uint8_t color1Green = (color1 >> 5) & 0b00111111;
    uint8_t color1Blue = color1 & 0b00011111;

    uint8_t color2Red = color2 >> 11;
    uint8_t color2Green = (color2 >> 5) & 0b00111111;
    uint8_t color2Blue = color2 & 0b00011111;

    for (uint8_t i = 0; i < 16; i++) {
        float alpha = i / 15.0f;
        float iAlpha = 1 - alpha;
        uint8_t blendRed = (color1Red * alpha) + (color2Red * iAlpha);
        uint8_t blendGreen = (color1Green * alpha) + (color2Green * iAlpha);
        uint8_t blendBlue = (color1Blue * alpha) + (color2Blue * iAlpha);
        __blend[i] = (blendRed << 11) + (blendGreen << 5) + blendBlue;
    }
*/
#if 1
    for (uint8_t i = 0; i < 16; i++) {

    // Alpha converted from [0..255] to [0..31]
    uint8_t alpha = i*2;

    // Converts  0000000000000000rrrrrggggggbbbbb
    //     into  00000gggggg00000rrrrr000000bbbbb
    // with mask 00000111111000001111100000011111
    // This is useful because it makes space for a parallel fixed-point multiply
    uint32_t bg = ((uint32_t)color2 | ((uint32_t)color2 << 16)) & 0b00000111111000001111100000011111;
    uint32_t fg = ((uint32_t)color1 | ((uint32_t)color1 << 16)) & 0b00000111111000001111100000011111;

    // This implements the linear interpolation formula: result = bg * (1.0 - alpha) + fg * alpha
    // This can be factorized into: result = bg + (fg - bg) * alpha
    // alpha is in Q1.5 format, so 0.0 is represented by 0, and 1.0 is represented by 32
    uint32_t result = (fg - bg) * alpha; // parallel fixed-point multiply of all components
    result >>= 5;
    result += bg;
    result &= 0b00000111111000001111100000011111; // mask out fractional parts
    __blend[i] = (uint16_t)((result >> 16) | result); // contract result

    }
#endif

    __fgColor = color1;
    __bgColor = color2;
}

void tftstCharHelper(TFTSTCustomFontCharData *charData) __z88dk_fastcall __naked
{
	#asm
	push d
	lxi b,3
	dad b		; HL+3 (charData.size)

	mov c,m
	inx h
	mov b,m		; BC = charData.size
	inx h		; HL+5 (chardata)

	mov e,m
	inx h
	mov d,m
	XCHG		; DE -> HL
			; HL = charData.compressedData[]

do_ex_char:

	push b

        mvi b,0
	mov a,m
	ani 00001111b
	rlc		; * 2
	mov c,a		; BC = aplha

	push h

	lxi h,___blend  ; HL = __blend[]
	dad b

	mov c,m		; BC = __blend[alpha]
	inx h
	mov b,m

	pop h

	mov a,m
	rar
	rar
	rar
	rar
	ani 00001111b
	mov d,a		; D = count
	inx h
	
do_ex_chr_cnt:
	mov a,b
	out LCD_DATA_PORT
	mov a,c
	out LCD_DATA_PORT
	dcr d
	jnz do_ex_chr_cnt

	pop b

	dcx b
	mov     a,b         
        ora     c           
        jnz     do_ex_char

	pop d
	ret
	#endasm
}

static uint8_t font_limit(TFTSTCustomFontData *font) { return font->size>100?58:font->size>40?57:126; }
void tftstDrawCharWithFont(TFTSTCustomFontData *font,uint16_t x,uint16_t y,uint8_t c,uint16_t color,uint16_t bg) {
    TFTSTCustomFontCharData *d;
    int16_t left;
    if (c<32 || c>font_limit(font)) return;
    d=font->charData+c-32;
    left=(int16_t)x+d->left;
    if (!d->width || !d->size || left<0 || left+d->width>lcddev.width || y+d->top>=lcddev.height) return;
    tfstPrepareBlend(color,bg);
    LCD_SetWindows(left,y+d->top,left+d->width-1,lcddev.height-1);
    tftstCharHelper(d);
}
void tftstDrawImg(char *img,uint16_t x,uint16_t y,uint16_t color,uint16_t bg) {
    uint16_t i;
    tfstPrepareBlend(color,bg);
    LCD_SetWindows(x,y,x+31,y+31);
    for(i=0;i<512;i++) {
        uint8_t v=(uint8_t)img[i];
        LCD_WR_DATA_16Bit(__blend[v>>4]);LCD_WR_DATA_16Bit(__blend[v&15]);
    }
}
void tftstDrawTextWithFont(TFTSTCustomFontData *font,uint16_t x,uint16_t y,char *text,uint16_t color,uint16_t bg) {
    uint8_t c,limit=font_limit(font);
    while ((c=(uint8_t)*text++)) {
        if (c<32 || c>limit) continue;
        if (x+font->charData[c-32].width+font->charData[c-32].left>lcddev.width) return;
        tftstDrawCharWithFont(font,x,y,c,color,bg);
        x+=font->charData[c-32].width+font->charData[c-32].left;
    }
}
void tftstDrawTextWithFontMulti(TFTSTCustomFontData *font,uint16_t x,uint16_t y,char *text,uint16_t color1,uint16_t color2,uint16_t bg) {
    uint8_t c,limit=font_limit(font);uint16_t color=color1;
    while ((c=(uint8_t)*text++)) {
        if(c==1){color=color1;continue;}
        if(c==2){color=color2;continue;}
        if(c<32 || c>limit) continue;
        tftstDrawCharWithFont(font,x,y,c,color,bg);
        x+=font->charData[c-32].width+font->charData[c-32].left;
    }
}
uint16_t tftstTextWithFontLength(TFTSTCustomFontData *font,char *text) {
    uint16_t len=0;uint8_t c,limit=font_limit(font);
    while ((c=(uint8_t)*text++)) if(c>=32 && c<=limit) len+=font->charData[c-32].width+font->charData[c-32].left;
    return len;
}
