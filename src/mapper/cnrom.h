/*****************
  Mapper 3: CNROM
 *****************/

#ifndef _CNROM_H
#define _CNROM_H

#include "../nes.h"
#include <iostream>
#include <iomanip>
using namespace std;

class cnrom : public mapper {
public:
  cnrom(nes *_nes) :_nes(_nes){
    for (int i=0;i<4;i++)
      _nes->get_mbc()->map_rom(i,i);
    for (int i=0;i<8;i++)
      _nes->get_mbc()->map_vrom(i,i);
  }

  void write(u16 adr,u8 dat){
    for (int i=0;i<8;i++)
      _nes->get_mbc()->map_vrom(i,dat*8+i);
  }

private:
  nes *_nes;
};

#endif
