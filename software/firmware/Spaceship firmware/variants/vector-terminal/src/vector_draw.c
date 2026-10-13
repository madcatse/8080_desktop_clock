/* Direct LCD Bresenham kept for ROM-level raster/reference tests.
 * Version 2 animation does NOT use this to erase/redraw old edges:
 * vector_step uses the mask renderer in vector_delta.c instead.
 */
#include "lcd.h"
#include "vector.h"
static uint16_t rx,ry,rc,err;
static uint8_t major,minor,count,steep,sx,sy;
/* Pixel windows written directly with canonical ports. Scratch is private
   to this non-reentrant step; no callback while an edge is being emitted.
   Only 8080 instructions; DE saved as required by sccz80's ABI. */
static void raster(void) __naked {
 #asm
 push d
 vr_pixel:
 mvi a,02ah
 out LCD_CMD_PORT
 lhld _rx
 mov a,h
 out LCD_DATA_PORT
 mov a,l
 out LCD_DATA_PORT
 mov a,h
 out LCD_DATA_PORT
 mov a,l
 out LCD_DATA_PORT
 mvi a,02bh
 out LCD_CMD_PORT
 lhld _ry
 mov a,h
 out LCD_DATA_PORT
 mov a,l
 out LCD_DATA_PORT
 mov a,h
 out LCD_DATA_PORT
 mov a,l
 out LCD_DATA_PORT
 mvi a,02ch
 out LCD_CMD_PORT
 lhld _rc
 mov a,h
 out LCD_DATA_PORT
 mov a,l
 out LCD_DATA_PORT
 lda _count
 dcr a
 sta _count
 jz vr_done
 lda _steep
 ora a
 jz vr_major_x
 call vr_y
 jmp vr_error
 vr_major_x:
 call vr_x
 vr_error:
 lda _minor
 mov e,a
 mvi d,0
 lhld _err
 mov a,l
 sub e
 mov l,a
 mov a,h
 sbb d
 mov h,a
 shld _err
 mov a,h
 ani 128
 jz vr_pixel
 lda _major
 mov e,a
 dad d
 shld _err
 lda _steep
 ora a
 jz vr_minor_y
 call vr_x
 jmp vr_pixel
 vr_minor_y:
 call vr_y
 jmp vr_pixel
 vr_done:
 pop d
 ret
 vr_x:
 lhld _rx
 lda _sx
 ora a
 jz vr_x_dec
 inx h
 jmp vr_x_store
 vr_x_dec:
 dcx h
 vr_x_store:
 shld _rx
 ret
 vr_y:
 lhld _ry
 lda _sy
 ora a
 jz vr_y_dec
 inx h
 jmp vr_y_store
 vr_y_dec:
 dcx h
 vr_y_store:
 shld _ry
 ret
 #endasm
}
void vector_draw(VLine *p,uint16_t color) {
 int16_t dx,dy;
 /* A corrupted line is rejected before any LCD traffic. */
 if(p->x0>=VIEW_W || p->x1>=VIEW_W || p->y0>=VIEW_H || p->y1>=VIEW_H)return;
 rx=VIEW_X+p->x0;ry=VIEW_Y+p->y0;rc=color;
 dx=(int16_t)p->x1-p->x0;dy=(int16_t)p->y1-p->y0;
 sx=dx>=0;sy=dy>=0;if(dx<0)dx=-dx;if(dy<0)dy=-dy;
 steep=dy>dx;major=steep?dy:dx;minor=steep?dx:dy;
 err=major/2;count=major+1;raster();
}
