#include "i8080.h"
#include <string.h>
#include <stdlib.h>
#define API __declspec(dllexport)
static i8080 c;
static uint8_t m[65536], ports[256], input[20000];
static uint16_t pixels[480*320];
static unsigned cmd, ix, sx, ex, sy, ey, x, y, color;
static unsigned writes, bad, lcd, rom, minsp, ilen, ipos, ip;
static unsigned piowrites,stops,rtc_reads,watched,wa,wn,prev1,prev2,laststop;
static unsigned wcount,wstate,wcycle[256],wvalue[256],pbout,pblatch,wread;
static unsigned long rtcbase, rtccycle;
static int autor;
static unsigned trace_on,trace_objects,trace_min,trace_writes,trace_same,trace_black;
static unsigned trace_target_valid;
static uint16_t trace_target[244*111];
static uint8_t rb(void*u,uint16_t a){return m[a];}
static void wb(void*u,uint16_t a,uint8_t v){if(a<32768)rom++;else m[a]=v;if(a>=wa&&a<wa+wn)watched++;}
static uint8_t bcd(unsigned n){return n/10*16+n%10;}
static uint8_t pi(void*u,uint8_t p){
 if(p==1)wread=c.cyc;
 if(p>=0x30&&p<=0x36)rtc_reads++;
 if(p==ip && ipos<ilen) return input[ipos++];
 if(autor && p>=0x30 && p<=0x32){unsigned s=rtcbase+(c.cyc-rtccycle)/3555555UL;
  return bcd(p==0x30?s%60:(p==0x31?s/60%60:s/3600%24));}
 return ports[p];
}
static void po(void*u,uint8_t p,uint8_t v){
 if(p<=3)piowrites++;
 if(p==1||(p==3&&(v&128))){
  if(p==1)pblatch=v&1;else{pbout=!(v&2);pblatch=0;}
  unsigned state=pbout?(pblatch?2:1):0;
  if(state!=wstate&&wcount<256){wcycle[wcount]=c.cyc;wvalue[wcount++]=state;wstate=state;}
 }
 if(p==3){if(prev2==0&&prev1==9&&v==1){stops++;laststop=c.cyc;}prev2=prev1;prev1=v;}
 if(p>=0x30&&p<=0x36)ports[p]=v;
 if(p==0x10){lcd++;cmd=v;ix=0;if(cmd==0x2c){x=sx;y=sy;}}
 else if(p==0x11){lcd++;ix++;
  if(cmd==0x2a||cmd==0x2b){unsigned*lo=cmd==0x2a?&sx:&sy,*hi=cmd==0x2a?&ex:&ey;
   if(ix==1)*lo=v<<8;if(ix==2)*lo|=v;if(ix==3)*hi=v<<8;if(ix==4)*hi|=v;
  }else if(cmd==0x2c){if(ix&1)color=v<<8;else{color|=v;
   if(x<480&&y<320){
    if(trace_on&&x>=218&&x<462&&y>=165&&y<276){
     unsigned prev=pixels[x+y*480],old_obj=prev==0x07ff||prev==0x87ff,new_obj=color==0x07ff||color==0x87ff;
     if(old_obj&&!new_obj)--trace_objects;if(!old_obj&&new_obj)++trace_objects;
     if(trace_objects<trace_min)trace_min=trace_objects;
     ++trace_writes;if(prev==color)++trace_same;
     if(trace_target_valid&&color==0&&trace_target[x-218+(y-165)*244]!=0)++trace_black;
    }
    pixels[x+y*480]=color;
   }else bad++;writes++;
   if(++x>ex){x=sx;y++;}}}
 }
}
API void reset(void){memset(m,0,sizeof m);memset(ports,255,sizeof ports);memset(pixels,0,sizeof pixels);
 i8080_init(&c);c.read_byte=rb;c.write_byte=wb;c.port_in=pi;c.port_out=po;
 writes=bad=lcd=rom=ilen=ipos=piowrites=stops=rtc_reads=watched=wa=wn=prev1=prev2=laststop=wcount=wstate=pbout=pblatch=wread=0;minsp=65535;autor=0;trace_on=trace_target_valid=0;}
API uint8_t* memory(void){return m;}
API uint16_t* screen(void){return pixels;}
API void port(unsigned p,unsigned v){ports[p]=v;}
API void script(unsigned p,uint8_t*b,unsigned n){ip=p;ilen=n;ipos=0;memcpy(input,b,n);}
API unsigned consumed(void){return ipos;}
API unsigned wire_count(void){return wcount;}
API unsigned wire_cycle(unsigned n){return wcycle[n];}
API unsigned wire_state(unsigned n){return wvalue[n];}
API unsigned wire_read_cycle(void){return wread;}
API void rtc_auto(unsigned s){autor=1;rtcbase=s;rtccycle=c.cyc;}
API void rtc_freeze(void){autor=0;}
API void scene_trace_start(void){
 trace_objects=trace_writes=trace_same=trace_black=trace_target_valid=0;
 for(unsigned yy=165;yy<276;yy++)for(unsigned xx=218;xx<462;xx++){
  unsigned p=pixels[xx+yy*480];if(p==0x07ff||p==0x87ff)++trace_objects;
 }
 trace_min=trace_objects;trace_on=1;
}
API void scene_trace_target(uint16_t *p){memcpy(trace_target,p,sizeof trace_target);trace_target_valid=1;}
API void scene_trace_stop(void){trace_on=0;}
API unsigned scene_trace_stat(unsigned n){return n==0?trace_min:n==1?trace_writes:n==2?trace_same:trace_black;}
API void watch(unsigned a,unsigned n){wa=a;wn=n;watched=0;}
API void stats_reset(void){writes=bad=lcd=rom=piowrites=stops=rtc_reads=watched=0;minsp=c.sp;}
API unsigned stat(unsigned n){return n==0?c.cyc:n==1?writes:n==2?lcd:n==3?bad:n==4?rom:n==5?minsp:n==6?c.sp:n==7?c.pc:n==8?(c.d*256+c.e)*65536u+c.h*256+c.l:n==9?c.h*256+c.l:n==10?piowrites:n==11?stops:n==12?watched:n==13?rtc_reads:n==14?laststop:0;}
API void prepare(unsigned pc,unsigned sp,unsigned hl){c.pc=pc;c.sp=sp;c.h=hl>>8;c.l=hl;c.halted=0;}
API int run(unsigned stop,unsigned cycles){unsigned long end=c.cyc+cycles;
 while(c.pc!=stop&&c.cyc<end&&!c.halted&&!rom){if(c.sp<minsp)minsp=c.sp;i8080_step(&c);}
 return c.pc==stop;
}
