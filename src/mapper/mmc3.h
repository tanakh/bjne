/****************
  Mapper 4: MMC3
 ****************/

#ifndef _MMC3_H
#define _MMC3_H

#include "../nes.h"
#include "../util.h"
#include <iostream>
#include <iomanip>
using namespace std;

class mmc3 : public mapper {
public:
  mmc3(nes *_nes) :_nes(_nes){
    rom_size=_nes->get_rom()->rom_size();
    prg_page[0]=0,prg_page[1]=1;
    chr_page[0]=0,chr_page[1]=1;
    chr_page[2]=2,chr_page[3]=3;
    chr_page[4]=4,chr_page[5]=5;
    chr_page[6]=6,chr_page[7]=7;

    prg_swap=false;
    chr_swap=false;
    
    set_rom();
    set_vrom();
  }

  void write(u16 adr,u8 dat){
    switch(adr&0xE001){
    case 0x8000:
      cmd=dat&7;
      prg_swap=_bit(dat,6);
      chr_swap=_bit(dat,7);
      break;

    case 0x8001:
      switch(cmd){
      case 0: // Select 2 1K VROM pages at PPU $0000
        chr_page[0]=dat&0xfe;
        chr_page[1]=(dat&0xfe)+1;
        set_vrom();
        break;
      case 1: // Select 2 1K VROM pages at PPU $0800
        chr_page[2]=dat&0xfe;
        chr_page[3]=(dat&0xfe)+1;
        set_vrom();
        break;
      case 2: // Select 1K VROM pages at PPU $1000
        chr_page[4]=dat;
        set_vrom();
        break;
      case 3: // Select 1K VROM pages at PPU $1400
        chr_page[5]=dat;
        set_vrom();
        break;
      case 4: // Select 1K VROM pages at PPU $1800
        chr_page[6]=dat;
        set_vrom();
        break;
      case 5: // Select 1K VROM pages at PPU $1C00
        chr_page[7]=dat;
        set_vrom();
        break;
      case 6: // Select first switchable ROM page
        prg_page[0]=dat;
        set_rom();
        break;
      case 7: // Select second switchable ROM page
        prg_page[1]=dat;
        set_rom();
        break;
      }
      break;

    case 0xA000:
      if (!_nes->get_rom()->is_four_screen())
        _nes->get_ppu()->set_mirroring((dat&1)?ppu::HORIZONTAL:ppu::VERTICAL);
      break;
    case 0xA001:
      if ((dat&0x80)!=0) ; // enable SRAM
      else ; // disable SRAM
      break;

    case 0xC000:
      irq_counter=dat;
      break;
    case 0xC001:
      irq_latch=dat;
      break;

    case 0xE000:
      irq_enable=false;
      irq_counter=irq_latch;
      break;
    case 0xE001:
      irq_enable=true;
      break;
    }
  }

  void hblank(int line){
    if (irq_enable &&
        line>=0&&line<239 &&
        _nes->get_regs()->draw_enabled()){

      if (!(irq_counter--)){
        irq_counter=irq_latch;
        _nes->get_cpu()->set_irq(true);
      }
    }
  }

  void serialize(state_data &sd){
    sd["MAPPER"]<<cmd<<prg_swap<<chr_swap
      <<irq_counter<<irq_latch<<irq_enable;
    for (int i=0;i<2;i++) sd["MAPPER"]<<prg_page[i];
    for (int i=0;i<8;i++) sd["MAPPER"]<<chr_page[i];
  }

private:
  void set_rom(){
    if (prg_swap){
      _nes->get_mbc()->map_rom(0,(rom_size-1)*2);
      _nes->get_mbc()->map_rom(1,prg_page[1]);
      _nes->get_mbc()->map_rom(2,prg_page[0]);
      _nes->get_mbc()->map_rom(3,(rom_size-1)*2+1);
    }
    else{
      _nes->get_mbc()->map_rom(0,prg_page[0]);
      _nes->get_mbc()->map_rom(1,prg_page[1]);
      _nes->get_mbc()->map_rom(2,(rom_size-1)*2);
      _nes->get_mbc()->map_rom(3,(rom_size-1)*2+1);
    }
  }
  void set_vrom(){
    for (int i=0;i<8;i++)
      _nes->get_mbc()->map_vrom((i+(chr_swap?4:0))%8,chr_page[i]);
  }

  int rom_size;
  
  int cmd;
  bool prg_swap,chr_swap;

  int irq_counter,irq_latch;
  bool irq_enable;

  int prg_page[2],chr_page[8];
  
  nes *_nes;
};

#endif
