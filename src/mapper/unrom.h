/*****************
  Mapper 2: UNROM
 *****************/

#ifndef _UNROM_H
#define _UNROM_H

#include "../nes.h"
#include <iostream>
#include <iomanip>
using namespace std;

class unrom : public mapper {
public:
  unrom(nes *_nes) :_nes(_nes){
    int rom_size=_nes->get_rom()->rom_size();
    _nes->get_mbc()->map_rom(0,0);
    _nes->get_mbc()->map_rom(1,1);
    _nes->get_mbc()->map_rom(2,(rom_size-1)*2);
    _nes->get_mbc()->map_rom(3,(rom_size-1)*2+1);
  }

  void write(u16 adr,u8 dat){
    _nes->get_mbc()->map_rom(0,dat*2);
    _nes->get_mbc()->map_rom(1,dat*2+1);
  }

private:
  nes *_nes;
};

#endif
