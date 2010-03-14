/****************
  Mapper 1: MMC1
 ****************/

#ifndef _MMC1_H
#define _MMC1_H

#include "../nes.h"
#include "../util.h"
#include <iostream>
#include <iomanip>
using namespace std;

class mmc1 : public mapper {
public:
  mmc1(nes *_nes) :_nes(_nes){
    rom_size=_nes->get_rom()->rom_size();
    size_in_kb=rom_size*16;
    
    cnt=buf=0;
    mirror=false;
    one_screen=false;
    switch_area=true;
    switch_size=true;
    vrom_switch_size=false;
    swap_base=0;
    vrom_page[0]=vrom_page[1]=1;
    rom_page[0]=0;rom_page[1]=rom_size-1;

    set_bank();
  }
  ~mmc1() {}

  void set_bank(){
    //cout<<rom_page[0]<<" "<<rom_page[1]<<endl;
    _nes->get_mbc()->map_rom(0,(swap_base|rom_page[0])*2);
    _nes->get_mbc()->map_rom(1,(swap_base|rom_page[0])*2+1);
    _nes->get_mbc()->map_rom(2,(swap_base|rom_page[1])*2);
    _nes->get_mbc()->map_rom(3,(swap_base|rom_page[1])*2+1);
  }
  void set_mirroring(){
    if (one_screen)
      _nes->get_ppu()->set_mirroring(mirror?ppu::HORIZONTAL:ppu::VERTICAL);
    else
      if (mirror)
        _nes->get_ppu()->set_mirroring(1,1,1,1);
      else
        _nes->get_ppu()->set_mirroring(0,0,0,0);
  }

  void write(u16 adr,u8 dat){
    // 1ビットづつ書き込まれ、バッファリングされる。5ビットたまったら動作する。
    if ((dat&0x80)!=0){
      cnt=0,buf=0;
      return;
    }
    buf|=((dat&1)<<cnt);
    cnt++;
    if (cnt<5) return;
    dat=(u8)buf;
    buf=cnt=0;
    
    //cout<<setfill('0')<<setw(4)<<hex<<adr<<" <- "<<setw(2)<<(int)dat<<dec<<endl;

    if (adr>=0x8000&&adr<=0x9FFF){ // reg0
      mirror          =_bit(dat,0);
      one_screen      =_bit(dat,1);
      switch_area     =_bit(dat,2);
      switch_size     =_bit(dat,3);
      vrom_switch_size=_bit(dat,4);

      set_mirroring();
    }
    else if (adr>=0xA000&&adr<=0xBFFF){ // reg1
      if (size_in_kb==512){
        swap_base=_bit(dat,4)?16:0;
        set_bank();
      }
      else if (size_in_kb==1024){
        // わからん…
      }

      vrom_page[0]=dat&0xf;
      if (vrom_switch_size){
        _nes->get_mbc()->map_vrom(0,vrom_page[0]*4);
        _nes->get_mbc()->map_vrom(1,vrom_page[0]*4+1);
        _nes->get_mbc()->map_vrom(2,vrom_page[0]*4+2);
        _nes->get_mbc()->map_vrom(3,vrom_page[0]*4+3);
      }
      else{
        _nes->get_mbc()->map_vrom(0,vrom_page[0]*4);
        _nes->get_mbc()->map_vrom(1,vrom_page[0]*4+1);
        _nes->get_mbc()->map_vrom(2,vrom_page[0]*4+2);
        _nes->get_mbc()->map_vrom(3,vrom_page[0]*4+3);
        _nes->get_mbc()->map_vrom(4,vrom_page[0]*4+4);
        _nes->get_mbc()->map_vrom(5,vrom_page[0]*4+5);
        _nes->get_mbc()->map_vrom(6,vrom_page[0]*4+6);
        _nes->get_mbc()->map_vrom(7,vrom_page[0]*4+7);
      }
    }
    else if (adr>=0xC000&&adr<=0xDFFF){ // reg2
      vrom_page[1]=dat&0xf;
      if (vrom_switch_size){
        _nes->get_mbc()->map_vrom(4,vrom_page[1]*4);
        _nes->get_mbc()->map_vrom(5,vrom_page[1]*4+1);
        _nes->get_mbc()->map_vrom(6,vrom_page[1]*4+2);
        _nes->get_mbc()->map_vrom(7,vrom_page[1]*4+3);
      }

      if (size_in_kb==1024){
        // わからん…
      }
    }
    else if (adr>=0xE000){ // reg3
      if (switch_size){ // 16K
        if (switch_area){
          rom_page[0]=dat&0xf;
          rom_page[1]=(rom_size-1)&0xf;
        }
        else{
          rom_page[0]=0;
          rom_page[1]=dat&0xf;
        }
      }
      else{ // 32K
        rom_page[0]=dat&0xe;
        rom_page[1]=(dat&0xe)|1;
      }
      set_bank();
    }
  }

  void serialize(state_data &sd){
    sd["MAPPER"]<<cnt<<buf
      <<mirror<<one_screen<<switch_area<<switch_size<<vrom_switch_size
      <<swap_base<<vrom_page[0]<<vrom_page[1]<<rom_page[0]<<rom_page[1];
  }

private:
  nes *_nes;

  int rom_size,size_in_kb;

  int cnt,buf;
  bool mirror,one_screen,switch_area,switch_size,vrom_switch_size;
  int swap_base,vrom_page[2],rom_page[2];
};

#endif
