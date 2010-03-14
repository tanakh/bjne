#include "nes.h"
#include "util.h"
#include <iostream>
#include <iomanip>
using namespace std;

regs::regs(nes *n) :_nes(n)
{
}

regs::~regs()
{
}

void regs::reset()
{
  nmi_enable=false;
  ppu_master=false; // unused
  sprite_size=false;
  bg_pat_adr=sprite_pat_adr=false;
  ppu_adr_incr=false;
  name_tbl_adr=0;
  
  bg_color=0;
  sprite_visible=bg_visible=false;
  sprite_clip=bg_clip=false;
  color_display=false;

  is_vblank=false;
  sprite0_occur=sprite_over=false;
  vram_write_flag=false;

  sprram_adr=0;
  ppu_adr_t=ppu_adr_v=ppu_adr_x=0;
  ppu_adr_toggle=false;
  ppu_read_buf=0;

  joypad_strobe=false;
  joypad_read_pos[0]=joypad_read_pos[1]=0;

  joypad_sign[0]=0x1,joypad_sign[1]=0x2;

  frame_irq=0xFF;
}

void regs::set_vblank(bool b,bool nmi)
{
  is_vblank=b;
  if (nmi&&(!is_vblank||nmi_enable))
    _nes->get_cpu()->set_nmi(is_vblank);
  if (is_vblank)
    sprite0_occur=false;
}

void regs::start_frame()
{
  if (bg_visible||sprite_visible)
    ppu_adr_v=ppu_adr_t;
}

void regs::start_scanline()
{
  if (bg_visible||sprite_visible)
    ppu_adr_v=(ppu_adr_v&0xfbe0)|(ppu_adr_t&0x041f);
}

void regs::end_scanline()
{
  if (bg_visible||sprite_visible){
    if (((ppu_adr_v>>12)&7)==7){
      ppu_adr_v&=~0x7000;
      if (((ppu_adr_v>>5)&0x1f)==29)
        ppu_adr_v=(ppu_adr_v&~0x03e0)^0x800;
      else if (((ppu_adr_v>>5)&0x1f)==31)
        ppu_adr_v&=~0x03e0;
      else
        ppu_adr_v+=0x20;
    }
    else
      ppu_adr_v+=0x1000;
  }
}

bool regs::draw_enabled()
{
  return sprite_visible||bg_visible;
}

void regs::set_input(int *dat)
{
  for (int i=0;i<16;i++)
    pad_dat[i/8][i%8]=dat[i]?true:false;
}

u8 regs::read(u16 adr)
{
  //cout<<"read reg #"<<hex<<setfill('0')<<setw(4)<<adr<<dec<<endl;

  if (adr>=0x4000){
    switch(adr){
    case 0x4016: // Joypad #1 (RW)
    case 0x4017: // Joypad #2 (RW)
      {
        int pad_num=adr-0x4016;
        int read_pos=joypad_read_pos[pad_num];
        u8 ret;
        if (read_pos<8) // パッドデータ
          ret=pad_dat[pad_num][read_pos]?1:0;
        else if (read_pos<16) // Ignored
          ret=0;
        else if (read_pos<20) // Signature
          ret=(joypad_sign[pad_num]>>(read_pos-16))&1;
        else
          ret=0;
        joypad_read_pos[pad_num]++;
        if (joypad_read_pos[pad_num]==24)
          joypad_read_pos[pad_num]=0;
        return ret;
      }
    case 0x4015:
      return _nes->get_apu()->read(0x4015)|(((frame_irq&0xC0)==0)?0x40:0);
    default:
      return _nes->get_apu()->read(adr);
    }
  }

  switch(adr&7){
  case 0: // PPU Control Register #1 (W)
  case 1: // PPU Control Register #2 (W)
  case 3: // SPR-RAM Address Register (W)
  case 4: // SPR-RAM I/O Register (W)
  case 5: // VRAM Address Register #1 (W2)
  case 6: // VRAM Address Register #2 (W2)
    cout<<"read #"<<setfill('0')<<hex<<setw(4)<<adr<<dec<<endl;
    return 0;

  case 2: { // PPU Status Register (R)
    u8 ret=_set(is_vblank,7)|_set(sprite0_occur,6)|_set(sprite_over,5)|_set(vram_write_flag,4);
    set_vblank(false,true);
    ppu_adr_toggle=false;
    return ret;
  }
  case 7: { // VRAM I/O Register (RW)
    u8 ret=ppu_read_buf;
    ppu_read_buf=read_2007();
    return ret; }
  }

  return 0;
}

void regs::write(u16 adr,u8 dat)
{
  //cout<<"write reg #"<<hex<<setfill('0')<<setw(4)<<adr<<" <- "<<setw(2)<<(int)dat<<dec<<endl;

  if (adr>=0x4000){
    switch(adr){
    case 0x4014: // Sprite DMA Register (W)
      {
        u8 *sprram=_nes->get_ppu()->get_sprite_ram();
        for (int i=0;i<0x100;i++)
          sprram[i]=_nes->get_mbc()->read((dat<<8)|i);
      }
      break;
    case 0x4016: // Joypad #1 (RW)
      {
        bool newval=(dat&1)!=0;
        if (joypad_strobe&&!newval) // たち下りエッジでリセット
          joypad_read_pos[0]=joypad_read_pos[1]=0;
        joypad_strobe=newval;
      }
      break;
    case 0x4017: // Joypad #2 (RW)
      frame_irq=dat;
      break;
    default:
      _nes->get_apu()->write(adr,dat);
      break;
    }
    return;
  }

  switch(adr&7){
  case 0: // PPU Control Register #1 (W)
    nmi_enable    =_bit(dat,7);
    ppu_master    =_bit(dat,6);
    sprite_size   =_bit(dat,5);
    bg_pat_adr    =_bit(dat,4);
    sprite_pat_adr=_bit(dat,3);
    ppu_adr_incr  =_bit(dat,2);
    //name_tbl_adr  =dat&3;
    ppu_adr_t=(ppu_adr_t&0xf3ff)|((dat&3)<<10);
    break;
  case 1: // PPU Control Register #2 (W)
    bg_color      =dat>>5;
    sprite_visible=_bit(dat,4);
    bg_visible    =_bit(dat,3);
    sprite_clip   =_bit(dat,2);
    bg_clip       =_bit(dat,1);
    color_display =_bit(dat,0);
    break;
  case 2: // PPU Status Register (R)
    // どうするか…
    cout<<"*** write to $2002"<<endl;
    break;
  case 3: // SPR-RAM Address Register (W)
    sprram_adr=dat;
    break;
  case 4: // SPR-RAM I/O Register (W)
    _nes->get_ppu()->get_sprite_ram()[sprram_adr++]=dat;
    break;
  case 5: // VRAM Address Register #1 (W2)
    ppu_adr_toggle=!ppu_adr_toggle;
    if (ppu_adr_toggle){
      ppu_adr_t=(ppu_adr_t&0xffe0)|(dat>>3);
      ppu_adr_x=dat&7;
    }
    else{
      ppu_adr_t=(ppu_adr_t&0xfC1f)|((dat>>3)<<5);
      ppu_adr_t=(ppu_adr_t&0x8fff)|((dat&7)<<12);
    }
    break;
  case 6: // VRAM Address Register #2 (W2)
    ppu_adr_toggle=!ppu_adr_toggle;
    if (ppu_adr_toggle)
      ppu_adr_t=(ppu_adr_t&0x00ff)|((dat&0x3f)<<8);
    else{
      ppu_adr_t=(ppu_adr_t&0xff00)|dat;
      ppu_adr_v=ppu_adr_t;
    }
    break;
  case 7: // VRAM I/O Register (RW)
    write_2007(dat);
    break;
  }
}

u8 regs::read_2007()
{
  u16 adr=ppu_adr_v&0x3fff;
  ppu_adr_v+=ppu_adr_incr?32:1;
  if (adr<0x2000)
    return _nes->get_mbc()->read_chr_rom(adr);
  else if (adr<0x3f00)
    return _nes->get_ppu()->get_name_page()[(adr>>10)&3][adr&0x3ff];
  else{
    if ((adr&3)==0) adr&=~0x10;
    return _nes->get_ppu()->get_palette()[adr&0x1f];
  }
}

void regs::write_2007(u8 dat)
{
  u16 adr=ppu_adr_v&0x3fff;
  ppu_adr_v+=ppu_adr_incr?32:1;
  if (adr<0x2000) // CHR-ROM
    _nes->get_mbc()->write_chr_rom(adr,dat);
  else if (adr<0x3f00) // name table
    _nes->get_ppu()->get_name_page()[(adr>>10)&3][adr&0x3ff]=dat;
  else{ // palette
    if ((adr&3)==0) adr&=~0x10; // mirroring
    _nes->get_ppu()->get_palette()[adr&0x1f]=dat&0x3f;
  }
}

void regs::serialize(state_data &sd)
{
  sd["REGS"]<<nmi_enable<<ppu_master<<sprite_size
    <<bg_pat_adr<<sprite_pat_adr<<ppu_adr_incr<<name_tbl_adr;
  sd["REGS"]<<bg_color<<sprite_visible<<bg_visible
    <<sprite_clip<<bg_clip<<color_display;
  sd["REGS"]<<is_vblank<<sprite0_occur<<sprite_over<<vram_write_flag;
  sd["REGS"]<<joypad_strobe
    <<joypad_read_pos[0]<<joypad_read_pos[1]
      <<joypad_sign[0]<<joypad_sign[1]<<frame_irq;
}
