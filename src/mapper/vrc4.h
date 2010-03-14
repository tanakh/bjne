/************************
  Mapper 25: Konami VRC4
 ************************/

#ifndef _VRC4_H
#define _VRC4_H

#include "../nes.h"
#include <iostream>
#include <iomanip>
using namespace std;

class vrc4 : public mapper {
public:
  vrc4(nes *_nes) :_nes(_nes){
    int rom_size=_nes->get_rom()->rom_size();
    _nes->get_mbc()->map_rom(0,0);
    _nes->get_mbc()->map_rom(1,1);
    _nes->get_mbc()->map_rom(2,(rom_size-1)*2);
    _nes->get_mbc()->map_rom(3,(rom_size-1)*2+1);
  }

  void write(u16 adr,u8 dat){
    switch(adr&0xF00F){
    case 0x8000:
      _nes->get_mbc()->map_rom(mswap?2:0,dat);
      rmap[mswap?1:0]=dat;
      break;
    case 0xA000:
      _nes->get_mbc()->map_rom(1,dat);
      break;

    case 0x9000:
      if ((dat&3)==0) _nes->get_ppu()->set_mirroring(ppu::VERTICAL);
      if ((dat&3)==1) _nes->get_ppu()->set_mirroring(ppu::HORIZONTAL);
      if ((dat&3)==2) _nes->get_ppu()->set_mirroring(0,0,0,0);
      if ((dat&3)==3) _nes->get_ppu()->set_mirroring(1,1,1,1);
      break;
    case 0x9001: case 0x9004: {
      bool nswap=(dat&2)!=0;
      if (mswap!=nswap){
        swap(rmap[0],rmap[1]);
        _nes->get_mbc()->map_rom(0,rmap[0]);
        _nes->get_mbc()->map_rom(2,rmap[1]);
      }
      mswap=nswap;
      break;
    }

    case 0xB000: case 0xB001: case 0xB004:
    case 0xC000: case 0xC001: case 0xC004:
    case 0xD000: case 0xD001: case 0xD004:
    case 0xE000: case 0xE001: case 0xE004:
      {
        int page=((adr>>12)-0xB)*2+(adr&1)+((adr>>2)&1);
        vmap[page]=(vmap[page]&0xF0)|(dat&0xF);
        _nes->get_mbc()->map_vrom(page,vmap[page]);
        break;
      }
    case 0xB002: case 0xB008: case 0xB003: case 0xB00C:
    case 0xC002: case 0xC008: case 0xC003: case 0xC00C:
    case 0xD002: case 0xD008: case 0xD003: case 0xD00C:
    case 0xE002: case 0xE008: case 0xE003: case 0xE00C:
      {
        int page=((adr>>12)-0xB)*2+(adr&1)+((adr>>2)&1);
        vmap[page]=(vmap[page]&0x0F)|((dat&0xF)<<4);
        _nes->get_mbc()->map_vrom(page,vmap[page]);
        break;
      }

    case 0xF000:
      irq_latch=(irq_latch&0xF0)|(dat&0xf);
      break;
    case 0xF002: case 0xF008:
      irq_latch=(irq_latch&0x0f)|((dat&0xf)<<4);
      break;
    case 0xF001: case 0xF004:
      irq_enable=dat&3;
      if (irq_enable&2)
        irq_count=irq_latch;
      break;
    case 0xF003: case 0xF00C:
      irq_enable=(irq_enable&1)?3:0;
      break;
    }
  }

  void hblank(int line){
    if (irq_enable&2){
      if (irq_count==0xff){
        _nes->get_cpu()->set_irq(true);
        irq_count=irq_latch;
      }
      else irq_count++;
    }
  }

  void serialize(state_data &sd){
    sd["MAPPER"]<<irq_latch<<irq_count<<irq_enable<<mswap;
    for (int i=0;i<8;i++) sd["MAPPER"]<<vmap[i];
    for (int i=0;i<2;i++) sd["MAPPER"]<<rmap[i];
  }

private:
  int irq_latch,irq_count,irq_enable;

  int vmap[8],rmap[2];
  bool mswap;

  nes *_nes;
};

#endif
