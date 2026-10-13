#ifndef VECTOR_H
#define VECTOR_H
#include <stdint.h>
/* Local animation coordinates. In the v2 pipeline only the delta emitter
   adds VIEW_X/VIEW_Y (the separate reference raster also adds this offset).
   Keep this rectangle separate from the clock, telemetry and frame brackets.
   Changing its size also requires updating the ASM stride/plane constants
   and right-edge guard in vector_delta.c; these are not generic bitmaps. */
#define VIEW_X 218
#define VIEW_Y 165
#define VIEW_W 244
#define VIEW_H 111
#define MAX_EDGES 24
#define NEAR_Z 16
/* Geometry uses signed integer coordinates; Q6 applies to sine/cosine only.
   VLine is a four-byte, already clipped segment in local pixel coordinates. */
typedef struct { int16_t x,y,z; } Vec3;
typedef struct { uint8_t x0,y0,x1,y1; } VLine;
/* model: 0=cube, 1=ship. A requested model is latched into frame_model at
   the next frame boundary. Pause finishes a transition already in progress. */
extern uint8_t model,paused,angle,vector_phase,frame_model;
extern uint16_t frames;
extern int16_t camera_z;
extern uint8_t previous_count,next_count;
/* Small edge lists are retained for diagnostics/reference raster tests.
   Version 2 removes pixels through scene masks, not by erasing these lines. */
extern VLine previous_lines[MAX_EDGES],next_lines[MAX_EDGES];
int16_t sine(uint8_t a);
void rotate_vertex(Vec3 *v,Vec3 *out,uint8_t yaw,uint8_t pitch);
uint8_t project_edge(Vec3 *a,Vec3 *b,VLine *out);
uint8_t clip_line(int16_t x0,int16_t y0,int16_t x1,int16_t y1,VLine *out);
void vector_init(void);
/* Cooperative operation: one vertex, one edge, one star or one bounded
   mask/delta chunk. Call repeatedly, with application service in between. */
void vector_step(void);
#endif
