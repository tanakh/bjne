#ifndef _PPU_H
#define _PPU_H

#include "renderer.h"
#include "serializer.h"

class nes;

class ppu{
public:
  ppu(nes *n);
  ~ppu();

  void reset();
  void render(int line,screen_info *scri);
  void sprite_check(int line);

  u8 *get_sprite_ram() { return sprram; }
  u8 *get_name_table() { return name_table; }
  u8 **get_name_page() { return name_page; }
  u8 *get_palette()    { return palette; }

  enum mirror_type{
    HORIZONTAL,VERTICAL,FOUR_SCREEN,SINGLE_SCREEN
  };
  void set_mirroring(mirror_type mt);
  void set_mirroring(int m0,int m1,int m2,int m3);

  void serialize(state_data &sd);

private:
  void render_bg(int line,u8 *buf);
  int render_spr(int line,u8 *buf);

  u8 sprram[0x100];
  u8 name_table[0x1000];
  u8 *name_page[4];
  u8 palette[0x20];

  u32 nes_palette_24[0x40];
  u16 nes_palette_16[0x40];
  
  nes *_nes;
};

#endif
