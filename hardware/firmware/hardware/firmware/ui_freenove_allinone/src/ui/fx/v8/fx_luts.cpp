#include "ui/fx/v8/fx_luts.h"
#include "ui/fx/v8/fx_utils.h"
#include <math.h>
#include <stdbool.h>

static int8_t s_sin[256], s_cos[256];
static uint8_t s_fade[256];
static uint16_t s_plasma[64], s_copper[256];
static bool s_init=false;

static inline uint16_t mk565(uint8_t r,uint8_t g,uint8_t b){ return fx_rgb565(r,g,b); }

void fx_luts_init(void){
  if(s_init) return;
  for(int i=0;i<256;i++){
    float a=(float)i*2.0f*3.1415926f/256.0f;
    s_sin[i]=(int8_t)lroundf(127.0f*sinf(a));
    s_cos[i]=(int8_t)lroundf(127.0f*cosf(a));
    float t=(float)i/255.0f;
    float f=t*t*(3.0f-2.0f*t);
    s_fade[i]=(uint8_t)lroundf(f*255.0f);
  }
  for(int i=0;i<64;i++){
    float t=(float)i/63.0f;
    float r=20+180*t;
    float g=30+210*(t*t);
    float b=60+195*(1-(1-t)*(1-t));
    float h=(t>0.85f)?(t-0.85f)/0.15f:0.0f;
    r=r+h*60; g=g+h*60; b=b+h*60;
    if(r>255)r=255;if(g>255)g=255;if(b>255)b=255;
    s_plasma[i]=mk565((uint8_t)r,(uint8_t)g,(uint8_t)b);
  }
  for(int i=0;i<256;i++){
    float t=(float)i/255.0f;
    float r=0.03f+0.97f*t;
    float g=0.02f+0.62f*t;
    float b=0.01f+0.22f*t;
    float h=(t>0.75f)?(t-0.75f)/0.25f:0.0f;
    r=r+h*0.25f; g=g+h*0.25f; b=b+h*0.25f;
    if(r>1)r=1;if(g>1)g=1;if(b>1)b=1;
    s_copper[i]=mk565((uint8_t)(r*255),(uint8_t)(g*255),(uint8_t)(b*255));
  }
  s_init=true;
}
int8_t fx_sin8(uint8_t p){ fx_luts_init(); return s_sin[p]; }
int8_t fx_cos8(uint8_t p){ fx_luts_init(); return s_cos[p]; }
uint8_t fx_fade_curve(uint8_t t){ fx_luts_init(); return s_fade[t]; }
uint16_t fx_palette_plasma565(uint8_t i6){ fx_luts_init(); return s_plasma[i6&63]; }
uint16_t fx_palette_copper565(uint8_t i){ fx_luts_init(); return s_copper[i]; }
