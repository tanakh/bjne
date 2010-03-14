#include "nes.h"
#include <memory.h>
#include <iostream>
#include <iomanip>
using namespace std;

ppu::ppu(nes *n) :_nes(n)
{
  static int pal_dat[0x40][3]={
    {0x75,0x75,0x75}, {0x27,0x1B,0x8F}, {0x00,0x00,0xAB}, {0x47,0x00,0x9F},
    {0x8F,0x00,0x77}, {0xAB,0x00,0x13}, {0xA7,0x00,0x00}, {0x7F,0x0B,0x00},
    {0x43,0x2F,0x00}, {0x00,0x47,0x00}, {0x00,0x51,0x00}, {0x00,0x3F,0x17},
    {0x1B,0x3F,0x5F}, {0x00,0x00,0x00}, {0x05,0x05,0x05}, {0x05,0x05,0x05},
    
    {0xBC,0xBC,0xBC}, {0x00,0x73,0xEF}, {0x23,0x3B,0xEF}, {0x83,0x00,0xF3},
    {0xBF,0x00,0xBF}, {0xE7,0x00,0x5B}, {0xDB,0x2B,0x00}, {0xCB,0x4F,0x0F},
    {0x8B,0x73,0x00}, {0x00,0x97,0x00}, {0x00,0xAB,0x00}, {0x00,0x93,0x3B},
    {0x00,0x83,0x8B}, {0x11,0x11,0x11}, {0x09,0x09,0x09}, {0x09,0x09,0x09},
    
    {0xFF,0xFF,0xFF}, {0x3F,0xBF,0xFF}, {0x5F,0x97,0xFF}, {0xA7,0x8B,0xFD},
    {0xF7,0x7B,0xFF}, {0xFF,0x77,0xB7}, {0xFF,0x77,0x63}, {0xFF,0x9B,0x3B},
    {0xF3,0xBF,0x3F}, {0x83,0xD3,0x13}, {0x4F,0xDF,0x4B}, {0x58,0xF8,0x98},
    {0x00,0xEB,0xDB}, {0x66,0x66,0x66}, {0x0D,0x0D,0x0D}, {0x0D,0x0D,0x0D},
    
    {0xFF,0xFF,0xFF}, {0xAB,0xE7,0xFF}, {0xC7,0xD7,0xFF}, {0xD7,0xCB,0xFF},
    {0xFF,0xC7,0xFF}, {0xFF,0xC7,0xDB}, {0xFF,0xBF,0xB3}, {0xFF,0xDB,0xAB},
    {0xFF,0xE7,0xA3}, {0xE3,0xFF,0xA3}, {0xAB,0xF3,0xBF}, {0xB3,0xFF,0xCF},
    {0x9F,0xFF,0xF3}, {0xDD,0xDD,0xDD}, {0x11,0x11,0x11}, {0x11,0x11,0x11}
  };

  for (int i=0;i<0x40;i++)
    nes_palette_24[i]=pal_dat[i][0]|(pal_dat[i][1]<<8)|(pal_dat[i][2]<<16);
}

ppu::~ppu()
{
}

void ppu::reset()
{
  memset(sprram,0,0x100);
  memset(name_table,0,0x1000);
  memset(palette,0,0x20);

  if (_nes->get_rom()->is_four_screen())
    set_mirroring(FOUR_SCREEN);
  else if (_nes->get_rom()->mirror()==rom::HORIZONTAL)
    set_mirroring(HORIZONTAL);
  else if (_nes->get_rom()->mirror()==rom::VERTICAL)
    set_mirroring(VERTICAL);
}

void ppu::set_mirroring(mirror_type mt)
{
  if (mt==HORIZONTAL)    set_mirroring(0,0,1,1);
  if (mt==VERTICAL)      set_mirroring(0,1,0,1);
  if (mt==FOUR_SCREEN)   set_mirroring(0,1,2,3);
  if (mt==SINGLE_SCREEN) set_mirroring(0,0,0,0);
}

void ppu::set_mirroring(int m0,int m1,int m2,int m3)
{
  name_page[0]=name_table+(m0&3)*0x400;
  name_page[1]=name_table+(m1&3)*0x400;
  name_page[2]=name_table+(m2&3)*0x400;
  name_page[3]=name_table+(m3&3)*0x400;
}

void ppu::render(int line,screen_info *scri)
{
  u8 buf[256+16]; // 前後に8ドット余裕を設けることにより処理の簡略化を図る
  u8 *p=buf+8;

  for (int i=0;i<256;i++) p[i]=palette[0];

  if (_nes->get_regs()->bg_visible)
    render_bg(line,p);
  if (_nes->get_regs()->sprite_visible)
    render_spr(line,p);

  if (scri->bpp==24||scri->bpp==32){ // フルカラー
    u8 *dest=(u8*)scri->buf+scri->pitch*line;
    int inc=scri->bpp/8;
    for (int i=0;i<256;i++){
      *(u32*)dest=nes_palette_24[p[i]&0x3f];
      dest+=inc;
    }
  }
  else if (scri->bpp==16){ // ハイカラー
    u16 *dest=(u16*)((u8*)scri->buf+scri->pitch*line);
    for (int i=0;i<256;i++)
      dest[i]=nes_palette_16[p[i]&0x3f];
  }
  else{
    // サポートしません。
  }
}

#define read_name_table(adr) (name_page[((adr)>>10)&3][(adr)&0x3ff])
#define read_pat_table(adr) (_nes->get_mbc()->read_chr_rom(adr))

void ppu::render_bg(int line,u8 *buf)
{
  regs *r=_nes->get_regs();
  int x_ofs=r->ppu_adr_x&7;
  int y_ofs=(r->ppu_adr_v>>12)&7;
  u16 name_adr=r->ppu_adr_v&0xfff;
  u16 pat_adr=r->bg_pat_adr?0x1000:0x0000;

  buf-=x_ofs;
  for (int i=0;i<33;i++,buf+=8){
    u8 tile=read_name_table(0x2000+name_adr);

    u8 l=read_pat_table(pat_adr+tile*16+y_ofs);
    u8 u=read_pat_table(pat_adr+tile*16+y_ofs+8);

    int tx=name_adr&0x1f,ty=(name_adr>>5)&0x1f;
    u16 attr_adr=(name_adr&0xC00)+0x3C0+((ty&~3)<<1)+(tx>>2);
    int aofs=((ty&2)==0?0:4)+((tx&2)==0?0:2);
    int attr=((read_name_table(0x2000+attr_adr)>>aofs)&3)<<2;

    for (int j=7;j>=0;j--){
      int t=((l&1)|(u<<1))&3;
      if (t!=0) buf[j]=0x40|palette[t|attr];
      l>>=1,u>>=1;
    }

    if ((name_adr&0x1f)==0x1f)
      name_adr=(name_adr&~0x1f)^0x400;
    else
      name_adr++;
  }
}

int ppu::render_spr(int line,u8 *buf)
{
  int spr_height=_nes->get_regs()->sprite_size?16:8;
  int pat_adr=_nes->get_regs()->sprite_pat_adr?0x1000:0x0000;
  
  for (int i=0;i<64;i++){
    int spr_y=sprram[i*4+0]+1;
    int attr=sprram[i*4+2];

    if (!(line>=spr_y&&line<spr_y+spr_height))
      continue;

    bool is_bg=((attr>>5)&1)!=0;
    int y_ofs=line-spr_y;
    int tile_index=sprram[i*4+1];
    int spr_x=sprram[i*4+3];
    int upper=(attr&3)<<2;

    bool h_flip=(attr&0x40)==0;
    int sx=h_flip?7:0,ex=h_flip?-1:8,ix=h_flip?-1:1;

    if ((attr&0x80)!=0)
      y_ofs=spr_height-1-y_ofs;

    u16 tile_adr;
    if (spr_height==16)
      tile_adr=(tile_index&~1)*16+((tile_index&1)*0x1000)+(y_ofs>=8?16:0)+(y_ofs&7);
    else
      tile_adr=pat_adr+tile_index*16+y_ofs;

    u8 l=read_pat_table(tile_adr);
    u8 u=read_pat_table(tile_adr+8);

    // 背面スプライトを描画されていた場合、その上にBGがかぶさっていても、
    // その上に前面スプライトが描画されることはない。
    for (int x=sx;x!=ex;x+=ix){
      int lower=(l&1)|((u&1)<<1);
      if (lower!=0&&(buf[spr_x+x]&0x80)==0){
        if (!is_bg||(buf[spr_x+x]&0x40)==0)
          buf[spr_x+x]=palette[0x10|upper|lower];
        buf[spr_x+x]|=0x80;
      }
      l>>=1,u>>=1;
    }
  }
}

void ppu::sprite_check(int line)
{
  // sprite 0 hit をチェックする。
  if (_nes->get_regs()->sprite_visible){
    int spr_y=sprram[0]+1;
    int spr_height=_nes->get_regs()->sprite_size?16:8;
    int pat_adr=_nes->get_regs()->sprite_pat_adr?0x1000:0x0000;

    if (line>=spr_y&&line<spr_y+spr_height){
      int y_ofs=line-spr_y;
      int tile_index=sprram[1];
      if ((sprram[2]&0x80)!=0)
        y_ofs=spr_height-1-y_ofs;
      u16 tile_adr;
      if (spr_height==16)
        tile_adr=(tile_index&~1)*16+((tile_index&1)*0x1000)+(y_ofs>=8?16:0)+(y_ofs&7);
      else
        tile_adr=pat_adr+tile_index*16+y_ofs;
      u8 l=read_pat_table(tile_adr);
      u8 u=read_pat_table(tile_adr+8);
      if (l!=0||u!=0)
        _nes->get_regs()->sprite0_occur=true;
    }
  }
}

void ppu::serialize(state_data &sd)
{
  for (int i=0;i<0x100;i++) sd["SPRITERAM"]<<sprram[i];
  for (int i=0;i<0x1000;i++) sd["NAMETABLE"]<<name_table[i];
  if (sd.is_reading()){
    for (int i=0;i<4;i++){
      int m;sd["MIRRORING"]<<m;
      name_page[i]=name_table+m%4*0x400;
    }
  }
  else{
    for (int i=0;i<4;i++)
      sd["MIRRORING"]<<(int)((name_page[i]-name_table)/0x400);
  }
  for (int i=0;i<0x20;i++) sd["PALETTE"]<<palette[i];
}
