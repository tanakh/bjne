/*******************************
  Mapper 23: Konami VRC2 type B
 *******************************/

#ifndef _VRC2B_H
#define _VRC2B_H

#include "../nes.h"
#include <iostream>
#include <iomanip>
using namespace std;

class vrc2b : public mapper {
public:
  vrc2b(nes *_nes) :_nes(_nes){
    int rom_size=_nes->get_rom()->rom_size();
    _nes->get_mbc()->map_rom(0,0);
    _nes->get_mbc()->map_rom(1,1);
    _nes->get_mbc()->map_rom(2,(rom_size-1)*2);
    _nes->get_mbc()->map_rom(3,(rom_size-1)*2+1);

    for (int i=0;i<8;i++) vmap[i]=i;
    swap=false;
    irq_enable=irq_latch=irq_count=0;
  }

  void write(u16 adr,u8 dat){
    switch(adr&0xF00C){
      // Select 8K ROM bank at $8000/$C000
    case 0x8000: case 0x8004: case 0x8008: case 0x800C:
      _nes->get_mbc()->map_rom(swap?2:0,dat);
      break;
      // Select 8K ROM bank at $A000
    case 0xA000: case 0xA004: case 0xA008: case 0xA00C:
      _nes->get_mbc()->map_rom(1,dat);
      break;

    case 0x9008:
      swap=(dat>>1)&1;
      break;

      // Select 1K VROM bank Low 4 bits
    case 0xB000: case 0xB002: case 0xB008:
    case 0xC000: case 0xC002: case 0xC008:
    case 0xD000: case 0xD002: case 0xD008:
    case 0xE000: case 0xE002: case 0xE008:
      {
        int bank=((adr>>12)-0xB)*2+((adr&2)>>1)+((adr&8)>>3);
        vmap[bank]=(vmap[bank]&0xf0)|(dat&0xf);
        _nes->get_mbc()->map_vrom(bank,vmap[bank]);
        break;
      }
      // Select 1K VROM bank High 4 bits
    case 0xB001: case 0xB004: case 0xB003: case 0xB00C:
    case 0xC001: case 0xC004: case 0xC003: case 0xC00C:
    case 0xD001: case 0xD004: case 0xD003: case 0xD00C:
    case 0xE001: case 0xE004: case 0xE003: case 0xE00C:
      {
        int bank=((adr>>12)-0xB)*2+((adr&2)>>1)+((adr&8)>>3);
        vmap[bank]=(vmap[bank]&0x0f)|((dat&0xf)<<4);
        _nes->get_mbc()->map_vrom(bank,vmap[bank]);
        break;
      }

    case 0x9000:
      if (dat!=0xFF){
        if ((dat&3)==0) _nes->get_ppu()->set_mirroring(ppu::VERTICAL);
        if ((dat&3)==1) _nes->get_ppu()->set_mirroring(ppu::HORIZONTAL);
        if ((dat&3)==2) _nes->get_ppu()->set_mirroring(0,0,0,0);
        if ((dat&3)==3) _nes->get_ppu()->set_mirroring(1,1,1,1);
      }
      break;

    case 0xF000:
      irq_latch=(irq_latch&0xF0)|(dat&0xF);
      break;
    case 0xF004:
      irq_latch=(irq_latch&0x0F)|((dat&0xF)<<4);
      break;
    case 0xF008:
      irq_enable=dat&3;
      if (irq_enable&2) irq_count=irq_latch;
      break;
    case 0xF00C:
      irq_enable=(irq_enable&1)*3;
      break;
    }
  }

  void hblank(int line){
    // VBlank ’†‚Å‚àƒCƒ“ƒNƒŠƒƒ“ƒg‚³‚ê‘±‚¯‚é
    if (irq_enable&2){
      if (irq_count>0xFF){
        _nes->get_cpu()->set_irq(true);
        irq_count=irq_latch;
        irq_enable=(irq_enable&1)*3;
      }
      else irq_count++;
    }
  }

  void serialize(state_data &sd){
    sd["MAPPER"]<<irq_latch<<irq_count<<irq_enable<<swap;
    for (int i=0;i<8;i++) sd["MAPPER"]<<vmap[i];
  }

private:
  int irq_latch,irq_count,irq_enable;

  int vmap[8];
  bool swap;
  
  nes *_nes;
};

#endif
