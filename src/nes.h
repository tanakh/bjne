#ifndef _NES_H
#define _NES_H

#include "types.h"
#include "rom.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "mbc.h"
#include "regs.h"
#include "mapper.h"
#include "renderer.h"
#include "serializer.h"

class nes{
public:
  nes(renderer *r);
  ~nes();

  bool load(const char *fname);
  bool check_mapper();
  bool save_sram(const char *fname);
  bool load_sram(const char *fname);
  bool save_state(const char *fname);
  bool load_state(const char *fname);

  void reset();
  void exec_frame();

  rom *get_rom() { return _rom; }
  cpu *get_cpu() { return _cpu; }
  ppu *get_ppu() { return _ppu; }
  apu *get_apu() { return _apu; }
  mbc *get_mbc() { return _mbc; }
  regs *get_regs() { return _regs; }
  mapper *get_mapper() { return _mapper; }

private:
  rom *_rom;
  cpu *_cpu;
  ppu *_ppu;
  apu *_apu;
  mbc *_mbc;
  regs *_regs;
  mapper *_mapper;

  renderer *_renderer;
};

#endif
