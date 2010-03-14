#ifndef _REGS_H
#define _REGS_H

#include "serializer.h"
class nes;

class regs{
  friend class nes;
  friend class ppu;
public:
  regs(nes *n);
  ~regs();

  void reset();

  u8 read(u16 adr);
  void write(u16 adr,u8 dat);

  void set_vblank(bool b,bool nmi);
  void start_frame();
  void start_scanline();
  void end_scanline();
  bool draw_enabled();

  void set_input(int *dat);

  void serialize(state_data &sd);

private:
  u8 read_2007();
  void write_2007(u8 dat);

  bool nmi_enable;
  bool ppu_master; // unused
  bool sprite_size;
  bool bg_pat_adr,sprite_pat_adr;
  bool ppu_adr_incr;
  int name_tbl_adr;
  
  int bg_color;
  bool sprite_visible,bg_visible;
  bool sprite_clip,bg_clip;
  bool color_display;

  bool is_vblank;
  bool sprite0_occur,sprite_over;
  bool vram_write_flag;

  u8 sprram_adr;
  u16 ppu_adr_t,ppu_adr_v,ppu_adr_x;
  bool ppu_adr_toggle;
  u8 ppu_read_buf;

  bool joypad_strobe;
  int joypad_read_pos[2],joypad_sign[2];
  int frame_irq;

  bool pad_dat[2][8];
  
  nes *_nes;
};

#endif
