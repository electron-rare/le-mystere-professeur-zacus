#include "ui/fx/v8/fx_sync.h"
void fx_sync_init(fx_sync_t* s, uint16_t bpm){
  s->bpm=bpm?bpm:120; s->t_ms=0; s->beat_index=0; s->beat_in_bar=0; s->bar_index=0;
  s->on_beat=false; s->on_bar=false; s->on_phrase=false;
}
void fx_sync_step(fx_sync_t* s, uint32_t dt_ms, uint8_t phrase_bars){
  if(!phrase_bars) phrase_bars=8;
  s->on_beat=s->on_bar=s->on_phrase=false;
  s->t_ms += dt_ms;
  uint32_t beat_ms=(uint32_t)(60000u/(uint32_t)s->bpm);
  uint32_t new_beat=s->t_ms/beat_ms;
  if(new_beat!=s->beat_index){
    s->beat_index=new_beat; s->on_beat=true;
    s->beat_in_bar=(uint8_t)(new_beat&3u);
    if(s->beat_in_bar==0){
      s->bar_index=(uint16_t)(new_beat>>2);
      s->on_bar=true;
      if((s->bar_index%phrase_bars)==0) s->on_phrase=true;
    }
  }
}
