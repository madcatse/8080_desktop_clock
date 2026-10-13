/* Vector-terminal v2: real 3-D geometry, cooperative frame preparation.
 *
 * Pipeline: model -> yaw/pitch -> camera translation -> near-plane clipping
 * -> perspective -> viewport clipping -> palette masks -> LCD difference.
 * There are no pre-rendered frames, floating-point operations or allocations.
 * Angles wrap at 64 steps per revolution; the sine table stores Q6 values.
 * See SOURCE.md for arithmetic bounds and vector_delta.c for output ordering.
 */
#include "vector.h"
#include "lcd.h"
#include "gui.h"
#include "vector_delta.h"
#include <string.h>

/* 17 entries for a quarter wave, 64 angular steps. */
static int8_t quarter[17]={0,6,12,19,24,30,36,41,45,49,53,56,59,61,63,64,64};
static Vec3 cube[8]={
 {-24,-24,-24},{24,-24,-24},{24,24,-24},{-24,24,-24},
 {-24,-24,24},{24,-24,24},{24,24,24},{-24,24,24}};
static uint8_t cube_edges[12][2]={
 {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
/* Dart: nose, two hull sections and two swept wings. Same pipeline. */
static Vec3 ship[11]={
 {0,0,46},{-7,-5,12},{7,-5,12},{7,5,12},{-7,5,12},
 {-10,-6,-24},{10,-6,-24},{10,6,-24},{-10,6,-24},{-38,0,-20},{38,0,-20}};
static uint8_t ship_edges[20][2]={
 {0,1},{0,2},{0,3},{0,4},{1,2},{2,3},{3,4},{4,1},
 {1,5},{2,6},{3,7},{4,8},{5,6},{6,7},{7,8},{8,5},
 {1,9},{5,9},{2,10},{6,10}};
static Vec3 transformed[16];
/* Cache [x, y, valid] for shared endpoints inside the viewport.
   An edge with any invalid endpoint takes the general clipping path. */
static uint8_t projected[16][3];
static uint8_t stars[12][2]={
 {12,11},{53,29},{92,7},{135,43},{181,18},{225,68},
 {24,89},{72,66},{111,95},{155,79},{196,102},{233,34}};
uint8_t model=1,paused,angle=6,vector_phase,frame_model;
uint16_t frames;
int16_t camera_z=144;
uint8_t previous_count,next_count;
VLine previous_lines[MAX_EDGES],next_lines[MAX_EDGES];
static uint8_t index;
static uint8_t vertex_count,edge_count;

int16_t sine(uint8_t a) {
 uint8_t q;
 /* Mirror a quarter wave, then apply the lower half-wave sign.
    cosine(a) is sine(a+16); masking handles angle wrapping. */
 a&=63;q=a&15;
 if(a&16) q=16-q;
 return a&32?-(int16_t)quarter[q]:quarter[q];
}
/* Contract: model coordinates <=48; each axis product sum <=12288.
   C /64 truncates toward zero, including negative values, on both hosts. */
void rotate_vertex(Vec3 *v,Vec3 *out,uint8_t yaw,uint8_t pitch) {
 int16_t s,c,x,z;
 if(v->x<-48 || v->x>48 || v->y<-48 || v->y>48 || v->z<-48 || v->z>48 || camera_z<0 || camera_z>256) {
  out->x=out->y=out->z=0;return;
 }
 s=sine(yaw);c=sine(yaw+16);
 /* Rotate around Y first, then X. Division, rather than a signed shift,
    gives matching truncation toward zero on the 8080 and test host. */
 x=(v->x*c+v->z*s)/64;
 z=(v->z*c-v->x*s)/64;
 s=sine(pitch);c=sine(pitch+16);
 out->x=x;
 out->y=(v->y*c-z*s)/64;
 out->z=(v->y*s+z*c)/64+camera_z;
}
static uint8_t code(int16_t x,int16_t y) {
 /* Cohen-Sutherland bits: left=1, right=2, top=4, bottom=8. */
 return (x<0?1:x>=VIEW_W?2:0)|(y<0?4:y>=VIEW_H?8:0);
}
/* Bounded Cohen-Sutherland. Inputs limited to +/-1100: 32-bit products
   cover all intersections; horizontal/vertical zero denominators reject.
   Eight iterations also bound work for adversarial rounding cases. */
uint8_t clip_line(int16_t x0,int16_t y0,int16_t x1,int16_t y1,VLine *out) {
 uint8_t a,b,c,n;int16_t x,y;
 if(x0<-1100 || x0>1100 || y0<-1100 || y0>1100 || x1<-1100 || x1>1100 || y1<-1100 || y1>1100) return 0;
 for(n=0;n<8;n++) {
  a=code(x0,y0);b=code(x1,y1);
  if(!(a|b)) {out->x0=x0;out->y0=y0;out->x1=x1;out->y1=y1;return 1;}
  if(a&b) return 0;
  c=a?a:b;
  if(c&12) {
   if(y1==y0) return 0;
   y=c&4?0:VIEW_H-1;
   x=x0+(int32_t)(x1-x0)*(y-y0)/(y1-y0);
  } else {
   if(x1==x0) return 0;
   x=c&1?0:VIEW_W-1;
   y=y0+(int32_t)(y1-y0)*(x-x0)/(x1-x0);
  }
  if(c==a) {x0=x;y0=y;} else {x1=x;y1=y;}
 }
 return 0;
}
static uint8_t in_domain(Vec3 *p) {
 return p->x>=-96 && p->x<=96 && p->y>=-96 && p->y<=96 && p->z>=-96 && p->z<=352;
}
uint8_t project_edge(Vec3 *a,Vec3 *b,VLine *out) {
 Vec3 p,q;int16_t x0,y0,x1,y1;
 if(!in_domain(a) || !in_domain(b)) return 0;
 p=*a;q=*b;
 /* Clip before perspective division. For a crossing edge, the two Z
    values straddle 16, so their difference cannot be zero. Cast BEFORE
    multiplying; assigning a 16-bit product to int32_t would be too late. */
 if(p.z<NEAR_Z && q.z<NEAR_Z) return 0;
 if(p.z<NEAR_Z) {
  p.x+=(int32_t)(q.x-p.x)*(NEAR_Z-p.z)/(q.z-p.z);
  p.y+=(int32_t)(q.y-p.y)*(NEAR_Z-p.z)/(q.z-p.z);p.z=NEAR_Z;
 } else if(q.z<NEAR_Z) {
  q.x+=(int32_t)(p.x-q.x)*(NEAR_Z-q.z)/(p.z-q.z);
  q.y+=(int32_t)(p.y-q.y)*(NEAR_Z-q.z)/(p.z-q.z);q.z=NEAR_Z;
 }
 /* z>=16, |axis*128|<=12288; no 16-bit overflow or divide by zero. */
 x0=VIEW_W/2+p.x*128/p.z;y0=VIEW_H/2-p.y*128/p.z;
 x1=VIEW_W/2+q.x*128/q.z;y1=VIEW_H/2-q.y*128/q.z;
 /* Near-plane extrema can reach 890 on X, within the clip domain. */
 return clip_line(x0,y0,x1,y1,out);
}
void vector_init(void) {vector_phase=0;previous_count=next_count=0;frames=0;delta_init();}
/* Build the entire next scene without LCD writes, then add/recolor before
   retiring obsolete pixels. Model/pause still take effect at boundaries.
   Phases 0..5 prepare in RAM; 6 adds/recolors; 7 removes and commits.
   Advance angle only after commit, so every vertex uses the same pose. */
void vector_step(void) {
 uint8_t a,b;int16_t x,y;VLine *p;
 if(!vector_phase) {
  /* A paused scene still gets one frame after a model selection. */
  if(paused && frames && frame_model==model) return;
  frame_model=model;vertex_count=model?11:8;edge_count=model?20:12;
  index=next_count=0;vector_phase=1;
 } else if(vector_phase==1) {
  rotate_vertex(frame_model?&ship[index]:&cube[index],&transformed[index],angle,angle/2+8);
  projected[index][2]=0;
  if(transformed[index].z>=NEAR_Z) {
   x=VIEW_W/2+transformed[index].x*128/transformed[index].z;
   y=VIEW_H/2-transformed[index].y*128/transformed[index].z;
   if(x>=0 && x<VIEW_W && y>=0 && y<VIEW_H) {
    projected[index][0]=x;projected[index][1]=y;projected[index][2]=1;
   }
  }
  if(++index==vertex_count) {index=0;vector_phase=2;}
 } else if(vector_phase==2) {
  a=frame_model?ship_edges[index][0]:cube_edges[index][0];
  b=frame_model?ship_edges[index][1]:cube_edges[index][1];p=&next_lines[next_count];
  /* Project a shared vertex once; general near/screen clipping remains
     available for an endpoint that falls outside the normal viewport. */
  if(projected[a][2] && projected[b][2]) {
   p->x0=projected[a][0];p->y0=projected[a][1];p->x1=projected[b][0];p->y1=projected[b][1];++next_count;
  } else if(project_edge(&transformed[a],&transformed[b],p)) ++next_count;
  if(++index==edge_count) {index=0;vector_phase=3;}
 } else if(vector_phase==3) {
  /* Clear the working mask in RAM; leave the visible old frame intact. */
  index=delta_clear_rows(index);
  if(index==VIEW_H) {index=0;vector_phase=4;}
 } else if(vector_phase==4) {
  /* A 2x2 star starts at y<=109, keeping its second row inside the view. */
  if(!paused) {if(++stars[index][1]>=VIEW_H-1) stars[index][1]=0;}
  delta_star(stars[index][0],stars[index][1],index&3?1:2);
  if(++index==12) {index=0;vector_phase=5;}
 } else if(vector_phase==5) {
  /* Model pixels overwrite stars in RAM, resolving intersection colors
     before either one reaches the LCD. */
  if(index<next_count) {delta_line(&next_lines[index],index&3?3:4);++index;}
  else {index=0;vector_phase=6;}
 } else if(vector_phase==6) {
  index=delta_emit_rows(0,index);
  if(index==VIEW_H) {index=0;vector_phase=7;}
 } else {
  index=delta_emit_rows(1,index);
  if(index==VIEW_H) {
   /* Swap scene ownership rather than copy 10 KB. The displayed mask
      must describe exactly what is visible before the next frame starts. */
   delta_commit();memcpy(previous_lines,next_lines,(uint16_t)next_count*4);
   previous_count=next_count;vector_phase=0;++frames;if(!paused)angle=(angle+1)&63;
  }
 }
}
