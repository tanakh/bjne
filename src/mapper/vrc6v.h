/*************************
  Mapper 26: Konami VRC6V
 *************************/

#ifndef _VRC6V_H
#define _VRC6V_H

#include "../nes.h"
#include <queue>
#include <iostream>
#include <iomanip>
using namespace std;

class vrc6v : public mapper {
public:
  vrc6v(nes *_nes) :_nes(_nes){
    int rom_size=_nes->get_rom()->rom_size();
    _nes->get_mbc()->map_rom(0,0);
    _nes->get_mbc()->map_rom(1,1);
    _nes->get_mbc()->map_rom(2,(rom_size-1)*2);
    _nes->get_mbc()->map_rom(3,(rom_size-1)*2+1);

    irq_count=0;
    irq_enable=0;
    irq_latch=0;
    bef_clk=0;
  }

  void write(u16 adr,u8 dat){
    switch(adr&0xF003){
    case 0x8000: // Select 16K ROM bank at $8000
      _nes->get_mbc()->map_rom(0,dat*2);
      _nes->get_mbc()->map_rom(1,dat*2+1);
      break;
    case 0xC000: // Select 8K ROM bank at $C000
      _nes->get_mbc()->map_rom(2,dat);
      break;

    case 0xD000: // Select 1K VROM bank at PPU $0000
      _nes->get_mbc()->map_vrom(0,dat);
      break;
    case 0xD001: // Select 1K VROM bank at PPU $0400
      _nes->get_mbc()->map_vrom(2,dat);
      break;
    case 0xD002: // Select 1K VROM bank at PPU $0800
      _nes->get_mbc()->map_vrom(1,dat);
      break;
    case 0xD003: // Select 1K VROM bank at PPU $0C00
      _nes->get_mbc()->map_vrom(3,dat);
      break;
    case 0xE000: // Select 1K VROM bank at PPU $1000
      _nes->get_mbc()->map_vrom(4,dat);
      break;
    case 0xE001: // Select 1K VROM bank at PPU $1400
      _nes->get_mbc()->map_vrom(6,dat);
      break;
    case 0xE002: // Select 1K VROM bank at PPU $1800
      _nes->get_mbc()->map_vrom(5,dat);
      break;
    case 0xE003: // Select 1K VROM bank at PPU $1C00
      _nes->get_mbc()->map_vrom(7,dat);
      break;

    case 0xB003:
      switch(dat&0x7f){
      case 0x08:
      case 0x2C:
        _nes->get_ppu()->set_mirroring(1,1,1,1);
        break;
      case 0x20:
        _nes->get_ppu()->set_mirroring(ppu::VERTICAL);
        break;
      case 0x24:
        _nes->get_ppu()->set_mirroring(ppu::HORIZONTAL);
        break;
      case 0x28:
        _nes->get_ppu()->set_mirroring(0,0,0,0);
        break;
      }
      if ((dat>>1)!=0){} // TODO : SRAM enable
      else {} // SRAM disable
      break;

    case 0xF000:
      irq_latch=dat;
      break;
    case 0xF001:
      irq_enable=dat&1;
      break;
    case 0xF002:
      irq_enable=dat&3;
      if (irq_enable&2) irq_count=irq_latch;
      break;

    case 0x9000: case 0x9001: case 0x9002:
    case 0xA000: case 0xA001: case 0xA002:
    case 0xB000: case 0xB001: case 0xB002:
      adr=(adr&0xfffc)|((adr&1)<<1)|((adr&2)>>1);
      write_queue.push(write_dat(_nes->get_cpu()->get_master_clock(),adr,dat));
      while(write_queue.size()>100){
        write_dat wd=write_queue.front();
        write_queue.pop();
        snd_write(wd.adr,wd.dat);
      }
      break;
    }
  }

  void snd_write(u16 adr,u8 dat){
    switch(adr&0xF003){
    case 0x9000: case 0xA000:
      sq[(adr>>12)-9].duty=((dat>>4)&7);
      sq[(adr>>12)-9].volume=dat&0xF;
      sq[(adr>>12)-9].gate=(dat>>7)!=0;
      break;
    case 0x9001: case 0xA001:
      sq[(adr>>12)-9].freq=(sq[(adr>>12)-9].freq&~0xFF)|dat;
      break;
    case 0x9002: case 0xA002:
      sq[(adr>>12)-9].freq=(sq[(adr>>12)-9].freq&0xFF)|((dat&0xF)<<8);
      sq[(adr>>12)-9].enable=(dat>>7)!=0;
      break;

    case 0xB000:
      saw.phase=dat&0x3F;
      break;
    case 0xB001:
      saw.freq=(saw.freq&~0xFF)|dat;
      break;
    case 0xB002:
      saw.freq=(saw.freq&0xFF)|((dat&0xF)<<8);
      saw.enable=(dat>>7)!=0;
      break;
    }
  }
  
  void hblank(int line){
    // VBlank 中でもインクリメントされ続ける
    if (irq_enable&3){
      if (irq_count>=0xFE){
        _nes->get_cpu()->set_irq(true);
        irq_count=irq_latch;
        irq_enable=0;
      }
      else irq_count++;
    }
  }

  void audio(sound_info *info){
    double cpu_clk=_nes->get_cpu()->get_frequency();
    double sample_clk=cpu_clk/info->freq;
    s64 cur_clk=_nes->get_cpu()->get_master_clock();

    for (int i=0;i<info->sample;i++){
      s64 cur=(cur_clk-bef_clk)*i/info->sample+bef_clk;
      while(!write_queue.empty()&&
            cur>=write_queue.front().clk){
        write_dat wd=write_queue.front();
        write_queue.pop();
        snd_write(wd.adr,wd.dat);
      }

      // サンプル(6ビット値)
      double d=(sq_produce(sq[0],sample_clk)+
                sq_produce(sq[1],sample_clk)+
                saw_produce(sample_clk))/32.0;

      if (info->bps==16)
        ((s16*)info->buf)[i]=(s16)(d*8000);
      else if (info->bps==8)
        ((u8*)info->buf)[i]=(u8)(d*30);
    }

    bef_clk=cur_clk;
  }

  void serialize(state_data &sd){
    while(!write_queue.empty()){
      write_dat wd=write_queue.front();
      write_queue.pop();
      snd_write(wd.adr,wd.dat);
    }

    sd["MAPPER"]<<irq_latch<<irq_count<<irq_enable<<bef_clk;
    for (int i=0;i<2;i++){
      sq_state &ss=sq[i];
      sd["MAPPER"]<<ss.duty<<ss.volume<<ss.gate<<ss.freq<<ss.enable<<ss.step;
    }
    sd["MAPPER"]<<saw.phase<<saw.freq<<saw.enable<<saw.step;
  }

private:
  int irq_latch,irq_count,irq_enable;

  struct sq_state{
    int duty,volume;
    bool gate;
    int freq;
    bool enable;

    int step;
    double clk;
  } sq[2];

  struct saw_state{
    int phase;
    int freq;
    bool enable;

    int step;
    double clk;
  } saw;

  int sq_produce(sq_state &sq,double clk){
    if (!sq.enable) return 0;
    if (sq.gate) return sq.volume;
    sq.clk+=clk;
    int adv=(int)(sq.clk/(sq.freq+1));
    sq.clk-=(sq.freq+1)*adv;
    sq.step=((sq.step+adv)%16);
    return ((sq.step>sq.duty)?1:0)*sq.volume;
  }
  int saw_produce(double clk){
    if (!saw.enable) return 0;
    saw.clk+=clk;
    int adv=(int)(saw.clk/(saw.freq+1));
    saw.clk-=(saw.freq+1)*adv;
    saw.step=((saw.step+adv)%7);
    return ((saw.step*saw.phase)>>3)&0x1f;
  }

  struct write_dat{
    write_dat(s64 c,u16 a,u8 d) :clk(c),adr(a),dat(d) {}
    s64 clk;
    u16 adr;
    u8 dat;
  };
  queue<write_dat> write_queue;

  s64 bef_clk;
  
  nes *_nes;
};

#endif
