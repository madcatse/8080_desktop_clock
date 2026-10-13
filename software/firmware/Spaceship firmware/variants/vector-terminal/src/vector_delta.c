/* Sparse palette scenes: construct in RAM, add/recolor, then retire pixels.
 * No LCD write during preparation; unchanged pixels never reach the bus.
 * Two three-bit palette scenes cost 20646 bytes in BSS; the LCD itself
 * holds the visible RGB565 frame. This is a small-window difference renderer.
 * Old and new edges may briefly coexist, but no whole-object erase occurs.
 */
#include <string.h>
#include "lcd.h"
#include "vector_delta.h"
/* Per row: inclusive [first occupied byte,last occupied byte].
   Empty sentinel is [MASK_STRIDE,0]; old/new span union bounds the scan. */
static uint8_t scene_bits[2][SCENE_BYTES],scene_spans[2][VIEW_H*2];
uint8_t *scene_old,*scene_new,*span_old,*span_new;
static uint16_t delta_palette[5]={0,0x4ad1,0xb7ff,0x07ff,0x87ff};
static uint8_t bit_values[8]={1,2,4,8,16,32,64,128};
/* ASM scratch avoids repeated C-stack argument decoding. bm_* belongs to
   mask Bresenham, dp_* to LCD differences, clr_* to clearing. Kernels never
   call application callbacks recursively. Naked routines preserve DE
   according to the sccz80 8080 ABI; HL/BC/A/flags are scratch registers. */
static uint16_t bm_ptr,bm_span,bm_err;
static uint8_t bm_bit,bm_byte,bm_major,bm_minor,bm_count,bm_steep,bm_sx,bm_sy,bm_c0,bm_c1,bm_c2;
static uint16_t dp_old,dp_new,dp_x,dp_y,dp_pixel_x;
/* Sparse lookup: only offsets for 1,2,4,8,16,32,64,128 are used.
   Extracting a single bit avoids a shifting loop for every changed pixel. */
static uint8_t bit_offsets[256]={0,0,1,0,2,0,0,0,3,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static uint8_t dp_nbytes,dp_pass,dp_o2,dp_n0,dp_n1,dp_n2,dp_delta,dp_bit;
static uint16_t clr_ptr,clr_groups;
static uint8_t clr_tail;

/* Eight stores per loop: bounded chunks, and no LCD traffic. */
static void clear_fast(void) __naked {
 #asm
 push d
 lhld _clr_groups
 mov b,h
 mov c,l
 lhld _clr_ptr
 mvi d,0
 mov a,b
 ora c
 jz cf_tail
 cf_eight:
 mov m,d
 inx h
 mov m,d
 inx h
 mov m,d
 inx h
 mov m,d
 inx h
 mov m,d
 inx h
 mov m,d
 inx h
 mov m,d
 inx h
 mov m,d
 inx h
 dcx b
 mov a,b
 ora c
 jnz cf_eight
 cf_tail:
 lda _clr_tail
 mov b,a
 ora a
 jz cf_done
 cf_one:
 mov m,d
 inx h
 dcr b
 jnz cf_one
 cf_done:
 pop d
 ret
 #endasm
}

/* Bresenham writes palette bits instead of LCD. Coordinates are clipped
   first. Row byte extents make the later difference scan sparse. */
static void bitmap_raster(void) __naked {
 #asm
 push d
 bm_pixel:
 /* Replace this pixel in all planes; clear then OR the new palette bit. */
 lda _bm_bit
 mov b,a
 cma
 mov c,a
 lhld _bm_ptr
 mov a,m
 ana c
 mov m,a
 lda _bm_c0
 ora a
 jz bm_plane1
 mov a,m
 ora b
 mov m,a
 bm_plane1:
 lxi d,3441
 dad d
 mov a,m
 ana c
 mov m,a
 lda _bm_c1
 ora a
 jz bm_plane2
 mov a,m
 ora b
 mov m,a
 bm_plane2:
 dad d
 mov a,m
 ana c
 mov m,a
 lda _bm_c2
 ora a
 jz bm_extent
 mov a,m
 ora b
 mov m,a
 bm_extent:
 lhld _bm_span
 lda _bm_byte
 cmp m
 jnc bm_max
 mov m,a
 bm_max:
 inx h
 cmp m
 jc bm_countdown
 mov m,a
 bm_countdown:
 lda _bm_count
 dcr a
 sta _bm_count
 jz bm_done
 lda _bm_steep
 ora a
 jz bm_major_x
 call bm_y
 jmp bm_error
 bm_major_x:
 call bm_x
 bm_error:
 /* Bresenham error is signed 16-bit; ANI 128 tests H's sign explicitly.
    Do not replace this with Z80-style JP: assembler semantics differ. */
 lda _bm_minor
 mov e,a
 mvi d,0
 lhld _bm_err
 mov a,l
 sub e
 mov l,a
 mov a,h
 sbb d
 mov h,a
 shld _bm_err
 ani 128
 jz bm_pixel
 lda _bm_major
 mov e,a
 dad d
 shld _bm_err
 lda _bm_steep
 ora a
 jz bm_minor_y
 call bm_x
 jmp bm_pixel
 bm_minor_y:
 call bm_y
 jmp bm_pixel
 bm_done:
 pop d
 ret
 bm_x:
 lda _bm_sx
 ora a
 jz bm_x_left
 lda _bm_bit
 rlc
 sta _bm_bit
 rnc
 lhld _bm_ptr
 inx h
 shld _bm_ptr
 lda _bm_byte
 inr a
 sta _bm_byte
 ret
 bm_x_left:
 lda _bm_bit
 rrc
 sta _bm_bit
 rnc
 lhld _bm_ptr
 dcx h
 shld _bm_ptr
 lda _bm_byte
 dcr a
 sta _bm_byte
 ret
 bm_y:
 lda _bm_sy
 ora a
 jz bm_y_up
 lxi d,31
 lhld _bm_ptr
 dad d
 shld _bm_ptr
 lhld _bm_span
 inx h
 inx h
 shld _bm_span
 ret
 bm_y_up:
 lxi d,-31
 lhld _bm_ptr
 dad d
 shld _bm_ptr
 lhld _bm_span
 dcx h
 dcx h
 shld _bm_span
 ret
 #endasm
}

void delta_line(VLine *p,uint8_t palette) {
 int16_t dx,dy;
 if(palette>4 || p->x0>=VIEW_W || p->x1>=VIEW_W || p->y0>=VIEW_H || p->y1>=VIEW_H)return;
 bm_byte=p->x0>>3;bm_bit=bit_values[p->x0&7];
 bm_ptr=(uint16_t)(scene_new+(uint16_t)p->y0*MASK_STRIDE+bm_byte);
 bm_span=(uint16_t)(span_new+(uint16_t)p->y0*2);
 bm_c0=palette&1;bm_c1=palette&2;bm_c2=palette&4;
 dx=(int16_t)p->x1-p->x0;dy=(int16_t)p->y1-p->y0;
 bm_sx=dx>=0;bm_sy=dy>=0;if(dx<0)dx=-dx;if(dy<0)dy=-dy;
 bm_steep=dy>dx;bm_major=bm_steep?dy:dx;bm_minor=bm_steep?dx:dy;
 /* Longest local line has major=243, count=244; uint8_t does not wrap. */
 bm_err=bm_major/2;bm_count=bm_major+1;bitmap_raster();
}
void delta_star(uint8_t x,uint8_t y,uint8_t palette) {
 VLine p;
 if(x>=VIEW_W-1 || y>=VIEW_H-1)return;
 /* Explicit field increments avoid sccz80's small-struct/stack peephole. */
 p.x0=x;p.x1=x;++p.x1;p.y0=y;p.y1=y;delta_line(&p,palette);
 ++p.y0;++p.y1;delta_line(&p,palette);
}
void delta_init(void) {
 uint8_t i;
 scene_old=scene_bits[0];scene_new=scene_bits[1];
 span_old=scene_spans[0];span_new=scene_spans[1];
 /* Clear both scenes once. Later frames clear only scene_new in chunks. */
 clr_ptr=(uint16_t)scene_bits;clr_groups=sizeof(scene_bits)>>3;clr_tail=sizeof(scene_bits)&7;clear_fast();
 for(i=0;i<VIEW_H;i++) {span_old[i*2]=span_new[i*2]=MASK_STRIDE;span_old[i*2+1]=span_new[i*2+1]=0;}
}
uint8_t delta_clear_rows(uint8_t row) {
 uint8_t n,i;uint16_t off,bytes;
 if(row>=VIEW_H)return VIEW_H;
 n=VIEW_H-row;if(n>16)n=16;
 off=(uint16_t)row*MASK_STRIDE;bytes=(uint16_t)n*MASK_STRIDE;
 clr_ptr=(uint16_t)(scene_new+off);clr_groups=bytes>>3;clr_tail=bytes&7;
 clear_fast();clr_ptr+=MASK_BYTES;clear_fast();clr_ptr+=MASK_BYTES;clear_fast();
 for(i=row;i<row+n;i++) {span_new[i*2]=MASK_STRIDE;span_new[i*2+1]=0;}
 return row+n;
}

/* Scan one bounded row extent. Pass 0 writes changed non-black pixels;
   pass 1 writes black ONLY where the complete new scene is black. This
   includes star/line intersections and changes between bright edge colors.
   Unchanged pixels are skipped bytewise, then bitwise. */
static void emit_row(void) __naked {
 #asm
 push d
 dp_byte:
 /* Difference = XOR(old0,new0) OR XOR(old1,new1) OR XOR(old2,new2).
    This catches color changes as well as appearing/disappearing pixels. */
 lhld _dp_old
 mov b,m
 lxi d,3441
 dad d
 mov c,m
 dad d
 mov a,m
 sta _dp_o2
 lhld _dp_new
 mov a,m
 sta _dp_n0
 xra b
 mov b,a
 dad d
 mov a,m
 sta _dp_n1
 xra c
 ora b
 mov b,a
 dad d
 mov a,m
 sta _dp_n2
 mov c,a
 lda _dp_o2
 xra c
 ora b
 mov b,a
 /* Candidate mask excludes the other pass before iterating pixels. */
 lda _dp_n0
 mov c,a
 lda _dp_n1
 ora c
 mov c,a
 lda _dp_n2
 ora c
 mov c,a
 lda _dp_pass
 ora a
 jz dp_candidates_add
 mov a,c
 cma
 ana b
 jmp dp_candidates_done
 dp_candidates_add:
 mov a,c
 ana b
 dp_candidates_done:
 sta _dp_delta
 ora a
 jz dp_skip_byte
 dp_pixel:
 /* Iterate set bits only; do not scan eight empty pixel positions. */
 /* bit = delta & -delta; remaining = delta XOR bit. */
 lda _dp_delta
 mov c,a
 cma
 inr a
 ana c
 sta _dp_bit
 mov b,a
 mov a,c
 xra b
 sta _dp_delta
 mov l,b
 mvi h,0
 lxi d,_bit_offsets
 dad d
 mov e,m
 mvi d,0
 lhld _dp_x
 dad d
 shld _dp_pixel_x
 mvi c,0
 lda _dp_pass
 ora a
 jnz dp_output
 lda _dp_n0
 ana b
 jz dp_color1
 mvi c,1
 dp_color1:
 lda _dp_n1
 ana b
 jz dp_color2
 mov a,c
 ori 2
 mov c,a
 dp_color2:
 lda _dp_n2
 ana b
 jz dp_pass_check
 mov a,c
 ori 4
 mov c,a
 dp_pass_check:
 mov a,c
 cpi 5
 jc dp_valid_color
 mvi c,0
 dp_valid_color:
 dp_output:
 /* Guard four padding bits in the final stride byte. */
 lhld _dp_pixel_x
 mov a,h
 cpi 1
 jc dp_in_view
 jnz dp_advance
 mov a,l
 cpi 206
 jnc dp_advance
 dp_in_view:
 mov l,c
 mvi h,0
 dad h
 lxi d,_delta_palette
 dad d
 mov e,m
 inx h
 mov d,m
 call dp_lcd
 dp_advance:
 lda _dp_delta
 ora a
 jnz dp_pixel
 dp_skip_byte:
 dp_next_byte:
 lhld _dp_x
 lxi d,8
 dad d
 shld _dp_x
 lhld _dp_old
 inx h
 shld _dp_old
 lhld _dp_new
 inx h
 shld _dp_new
 lda _dp_nbytes
 dcr a
 sta _dp_nbytes
 jnz dp_byte
 pop d
 ret
 dp_lcd:
 /* ILI9486-style commands: 2A column, 2B row, 2C RGB565 write.
    Window endpoints are inclusive; one pixel sends high byte then low.
    dp_pixel_x/dp_y are absolute LCD coordinates, color is passed in DE. */
 mvi a,02ah
 out LCD_CMD_PORT
 lhld _dp_pixel_x
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
 lhld _dp_y
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
 mov a,d
 out LCD_DATA_PORT
 mov a,e
 out LCD_DATA_PORT
 ret
 #endasm
}
uint8_t delta_emit_rows(uint8_t pass,uint8_t row) {
 uint8_t scan=0,paint=0,lo,hi;uint16_t off;
 dp_pass=pass;
 /* Up to 16 inspected rows, at most 4 nonempty row extents per step.
    Worst LCD budget is 4*244 pixels; ordinary wireframe rows are sparse. */
 while(row<VIEW_H && scan<16 && paint<4) {
  lo=span_old[row*2];if(span_new[row*2]<lo)lo=span_new[row*2];
  hi=span_old[row*2+1];if(span_new[row*2+1]>hi)hi=span_new[row*2+1];
  if(lo<MASK_STRIDE && hi<MASK_STRIDE && hi>=lo) {
   off=(uint16_t)row*MASK_STRIDE+lo;
   dp_old=(uint16_t)(scene_old+off);dp_new=(uint16_t)(scene_new+off);
   dp_x=VIEW_X+(uint16_t)lo*8;dp_y=VIEW_Y+row;dp_nbytes=hi-lo+1;
   emit_row();++paint;
  }
  ++row;++scan;
 }
 return row;
}
void delta_commit(void) {
 /* Called after every required LCD pixel was written. Pointer swaps
    retain the complete displayed scene without a memcpy or screen erase. */
 uint8_t *p;
 p=scene_old;scene_old=scene_new;scene_new=p;
 p=span_old;span_old=span_new;span_new=p;
}
