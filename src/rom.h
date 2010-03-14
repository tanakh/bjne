#ifndef _ROM_H
#define _ROM_H

#include "types.h"

class rom{
public:
  rom();
  ~rom();

  void reset();
  void release();
  bool load(const char *fname);
  bool save_sram(const char *fname);
  bool load_sram(const char *fname);

  u8 *get_rom() { return rom_dat; }
  u8 *get_chr() { return chr_dat; }
  u8 *get_sram(){ return sram; }
  u8 *get_vram(){ return vram; }

  int rom_size() { return prg_page_cnt; }
  int chr_size() { return chr_page_cnt; }
  int mapper_no() { return mapper; }

  bool has_sram() { return sram_enable; }
  bool has_trainer() { return trainer_enable; }
  bool is_four_screen() { return four_screen; }

  enum mirror_type{
    HORIZONTAL,VERTICAL,
  };

  mirror_type mirror() { return mirroring; }

private:
  int prg_page_cnt,chr_page_cnt;

  mirror_type mirroring;

  bool sram_enable,trainer_enable;
  bool four_screen;
  int mapper;
  
  u8 *rom_dat,*chr_dat,*sram,*vram;
};

#endif
