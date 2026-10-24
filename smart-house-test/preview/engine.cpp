#include "../effects.h"
static House::Engine engine;
static House::Config cfg;
extern "C" {
void settings(int mode,int speed,int density){cfg.mode=House::Mode(mode);cfg.speed=speed;cfg.density=density;}
void floor_settings(int floor,int on,int color){
 if(floor<0 || floor>2)return;
 cfg.on[floor]=on;
 cfg.colors[floor]={uint8_t(color>>16),uint8_t(color>>8),uint8_t(color)};
}
void render(unsigned now){engine.render(now,cfg);}
int pixel(int floor,int index){
 if(floor<0 || floor>2 || index<0 || index>=House::LEDS_PER_FLOOR)return 0;
 auto c=engine.pixels[floor][index];return (int(c.r)<<16)|(int(c.g)<<8)|c.b;
}
}
