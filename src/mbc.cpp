#include "nes.h"
#include <memory.h>
using namespace std;

mbc::mbc(nes *n) :_nes(n)
{
  rom=vrom=ram=NULL;
  is_vram=false;
  ram=new u8[0x800];
}

mbc::~mbc()
{
  delete []ram;
}

void mbc::reset()
{
  memset(ram,0,0x800);
  
  rom=_nes->get_rom()->get_rom();
  vrom=_nes->get_rom()->get_chr();
  sram=_nes->get_rom()->get_sram();
  vram=_nes->get_rom()->get_vram();

  for (int i=0;i<4;i++) map_rom(i,i);
  if (vrom!=NULL)
    for (int i=0;i<8;i++) map_vrom(i,i);
  else
    for (int i=0;i<8;i++) map_vram(i,i);
  sram_enabled=true;
}

void mbc::map_rom(int page,int val)
{
  rom_page[page]=rom+(val%(_nes->get_rom()->rom_size()*2))*0x2000;
}

void mbc::map_vrom(int page,int val)
{
  if (vrom!=NULL){
    chr_page[page]=vrom+(val%(_nes->get_rom()->chr_size()*8))*0x0400;
    is_vram=false;
  }
}

void mbc::map_vram(int page,int val)
{
  // TODO : VRAM ÉTÉCÉY
  chr_page[page]=vram+(val%8)*0x400;
  is_vram=true;
}

u8 mbc::read(u16 adr)
{
  switch(adr>>11){
  case 0x00: case 0x01: case 0x02: case 0x03: // 0x0000Å`0x1FFF
    return ram[adr&0x7ff];
  case 0x04: case 0x05: case 0x06: case 0x07: // 0x2000Å`0x3FFF
    return _nes->get_regs()->read(adr);
  case 0x08: case 0x09: case 0x0A: case 0x0B: // 0x4000Å`0x5FFF
    if (adr<0x4020) return _nes->get_regs()->read(adr);
    return 0; // TODO : Expanision ROM ???
  case 0x0C: case 0x0D: case 0x0E: case 0x0F: // 0x6000Å`0x7FFF
    if (sram_enabled)
      return sram[adr&0x1fff];
    else
      return 0x00; // ???

  case 0x10: case 0x11: case 0x12: case 0x13: // 0x8000Å`0x9FFF
    return rom_page[0][adr&0x1fff];
  case 0x14: case 0x15: case 0x16: case 0x17: // 0xA000Å`0xBFFF
    return rom_page[1][adr&0x1fff];
  case 0x18: case 0x19: case 0x1A: case 0x1B: // 0xC000Å`0xDFFF
    return rom_page[2][adr&0x1fff];
  case 0x1C: case 0x1D: case 0x1E: case 0x1F: // 0xE000Å`0xFFFF
    return rom_page[3][adr&0x1fff];
  }
}

void mbc::write(u16 adr,u8 dat)
{
  switch(adr>>11){
  case 0x00: case 0x01: case 0x02: case 0x03: // 0x0000Å`0x1FFF
    ram[adr&0x7ff]=dat;
    break;
  case 0x04: case 0x05: case 0x06: case 0x07: // 0x2000Å`0x3FFF
    _nes->get_regs()->write(adr,dat);
    break;
  case 0x08: case 0x09: case 0x0A: case 0x0B: // 0x4000Å`0x5FFF
    if (adr<0x4020) _nes->get_regs()->write(adr,dat);
    else if (_nes->get_mapper())
      _nes->get_mapper()->write(adr,dat);
    break;
  case 0x0C: case 0x0D: case 0x0E: case 0x0F: // 0x6000Å`0x7FFF
    if (sram_enabled)
      sram[adr&0x1fff]=dat;
    else if (_nes->get_mapper())
      _nes->get_mapper()->write(adr,dat);
    break;

  case 0x10: case 0x11: case 0x12: case 0x13: // 0x8000Å`0xFFFF
  case 0x14: case 0x15: case 0x16: case 0x17:
  case 0x18: case 0x19: case 0x1A: case 0x1B:
  case 0x1C: case 0x1D: case 0x1E: case 0x1F:
    if (_nes->get_mapper())
      _nes->get_mapper()->write(adr,dat);
    break;
  }
}

u8 mbc::read_chr_rom(u16 adr)
{
  return chr_page[(adr>>10)&7][adr&0x03ff];
}

void mbc::write_chr_rom(u16 adr,u8 dat)
{
  chr_page[(adr>>10)&7][adr&0x03ff]=dat;
}

void mbc::serialize(state_data &sd)
{
  if (sd.is_reading()){
    int t;
    for (int i=0;i<4;i++){
      sd["MAPPING"]<<t;
      map_rom(i,t);
    }
    sd["MAPPING"]<<is_vram;
    for (int i=0;i<8;i++){
      sd["MAPPING"]<<t;
      if (is_vram) map_vram(i,t);
      else map_vrom(i,t);
    }
  }
  else{
    for (int i=0;i<4;i++)
      sd["MAPPING"]<<(int)((rom_page[i]-rom)/0x2000);
    sd["MAPPING"]<<is_vram;
    for (int i=0;i<8;i++)
      sd["MAPPING"]<<(int)((chr_page[i]-(is_vram?vram:vrom))/0x400);
  }
  for (int i=0;i<0x800;i++) sd["RAM"]<<ram[i];
  for (int i=0;i<0x2000;i++) sd["SRAM"]<<sram[i];
  for (int i=0;i<0x2000;i++) sd["VRAM"]<<vram[i];
  sd["SRAM"]<<sram_enabled;
}
