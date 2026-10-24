#pragma once
#if defined(__wasm__)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
#else
#include <stdint.h>
#endif

// Portable effect engine, compiled unchanged for ESP32 and the local preview.
namespace House {
struct RGB { uint8_t r,g,b; };
enum Mode { Manual, Cycle, Snow, Halloween, Christmas, Matrix };
constexpr int LEDS_PER_FLOOR = 10;
struct Config {
  Mode mode=Cycle;
  bool on[3]={true,true,true};
  RGB colors[3]={{255,160,75},{255,160,75},{255,160,75}};
  uint8_t speed=3,density=4;
};
inline uint32_t hash(uint32_t v) {v^=v>>16;v*=0x7feb352dUL;v^=v>>15;v*=0x846ca68bUL;return v^(v>>16);}
inline RGB mix(RGB a,RGB b,uint8_t t){
 return {uint8_t((a.r*(255-t)+b.r*t)/255),uint8_t((a.g*(255-t)+b.g*t)/255),uint8_t((a.b*(255-t)+b.b*t)/255)};
}
inline RGB scale(RGB c,uint8_t v){return {uint8_t(c.r*v/255),uint8_t(c.g*v/255),uint8_t(c.b*v/255)};}
inline uint8_t sat(unsigned v){return v>255?255:v;}
inline RGB add(RGB a,RGB b){return {sat(a.r+b.r),sat(a.g+b.g),sat(a.b+b.b)};}
inline RGB hue(uint16_t h){
 uint8_t up=(h%60)*255/60,down=255-up;
 switch((h/60)%6){case 0:return {255,up,0};case 1:return {down,255,0};case 2:return {0,255,up};case 3:return {0,down,255};case 4:return {up,0,255};default:return {255,0,down};}
}
struct Engine {
 uint32_t seed=0x43f812ad, nextEvent=0, eventStart=0;
 uint16_t flashMs=80;
 uint8_t eventFloor=0,flashes=0;
 bool blackout=false;
 uint32_t random(){seed=hash(seed+0x9e3779b9UL);return seed;}
 RGB pixels[3][LEDS_PER_FLOOR]={};
 void render(uint32_t now,const Config& cfg){
   if(cfg.mode==Halloween && (!nextEvent || int32_t(now-nextEvent)>=0)){
     eventStart=now; eventFloor=random()%3; flashes=1+random()%5;
     flashMs=45+random()%110; blackout=(random()%3)==0;
     nextEvent=now+flashes*flashMs*2+400+random()%4500;
   }
   for(int f=0;f<3;++f){
     int count=LEDS_PER_FLOOR;
     for(int p=0;p<count;++p){
       RGB c=cfg.colors[f];
       if(cfg.mode==Cycle) c=hue((now%8000)*360/8000);
       if(cfg.mode==Snow){
         c={0,3,12};
         uint32_t duration=8000-cfg.speed*1000;
         for(int n=0;n<cfg.density;++n){
           uint32_t shifted=now+duration*n/cfg.density;
           uint32_t phase=(shifted%duration)*768/duration;
           uint32_t pos=((hash(n*2654435761UL+(shifted/duration)*2246822519UL)&255)*count)/256;
           if(phase/256==uint32_t(f) && (pos==uint32_t(p) || pos+1==uint32_t(p))){
             uint8_t t=phase%256; uint8_t bright=32+(t<128?t:255-t)*223/127;
             RGB white=scale({220,235,255},bright);
             if(pos+1==uint32_t(p))white=scale(white,60);
             c=add(c,white);
           }
         }
       }
       if(cfg.mode==Halloween){
         // Candle noise and orange/purple drift do not schedule the flash events.
         uint8_t drift=hash(now/1900+f*179)%220;
         uint8_t candle=130+hash(now/67+p*43+f*101)%126;
         c=scale(mix({38,0,68},{255,64,0},drift),candle);
         uint32_t elapsed=now-eventStart;
         if(f==eventFloor && elapsed<uint32_t(flashes)*flashMs*2){
           c=blackout || (elapsed/flashMs)%2 ? RGB{0,0,0}:RGB{255,210,155};
         }
       }
       if(cfg.mode==Matrix){
         // One horizontal stream per floor, with independent timing and a dark gap.
         const uint32_t stepMs=110+f*35;
         const uint32_t tick=now/stepMs+f*7;
         const int head=tick%(count+7);
         const int behind=head-p;
         c={0,0,0};
         if(behind==0) c={155,255,185};
         else if(behind>0 && behind<=5){
           const uint8_t trail[]={0,210,130,72,34,12};
           // Subtle digital flicker, always dimmer than the leading pixel.
           const uint8_t flicker=185+hash(now/90+p*43+f*101)%71;
           c=scale({0,trail[behind],3},flicker);
         }
       }
       if(cfg.mode==Christmas){
         uint32_t phase=(now+f*720)%4800;
         uint8_t blend=phase<1800?0:phase<2400?(phase-1800)*255/600:phase<4200?255:255-(phase-4200)*255/600;
         c=mix({220,0,12},{0,180,32},blend);
         uint32_t sparkle=hash(now/120+p*797+f*151);
         if(sparkle%11<2) c=mix(c,sparkle&16?RGB{255,205,65}:RGB{255,255,235},140+sparkle%116);
       }
       pixels[f][p]=cfg.on[f]?c:RGB{0,0,0};
     }
   }
 }
};
}
