#ifndef VECTOR_DELTA_H
#define VECTOR_DELTA_H
#include "vector.h"
/* ceil(244/8)=31 bytes/row; bits 4..7 of the last byte are unused.
   31*111=3441 bytes/plane, three planes/scene. Constants are mirrored
   by immediate operands in the 8080 ASM; change them together. */
#define MASK_STRIDE 31
#define MASK_BYTES 3441
#define SCENE_BYTES 10323
/* Three one-bit layers encode the five original scene colors. */
/* Palette indices: 0 black, 1 dim star, 2 bright star, 3 edge, 4 bright edge.
   Index bit k is in plane k; pixel x uses bit (x & 7), least bit first. */
extern uint8_t *scene_old,*scene_new,*span_old,*span_new;
/* Requires an already black viewport (terminal_init).
   Scratch state is non-reentrant: use only from the cooperative main loop. */
void delta_init(void);
/* Row functions return the next cursor; VIEW_H means the pass finished. */
uint8_t delta_clear_rows(uint8_t row);
void delta_line(VLine *p,uint8_t palette);
void delta_star(uint8_t x,uint8_t y,uint8_t palette);
uint8_t delta_emit_rows(uint8_t pass,uint8_t row);
/* Commit only AFTER BOTH emission passes reach VIEW_H. */
void delta_commit(void);
#endif
