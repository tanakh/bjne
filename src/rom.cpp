#include "rom.h"
#include <iostream>
#include <algorithm>
#include <cstdio>
#include <cstring>
using namespace std;

rom::rom()
{
  rom_dat=NULL,chr_dat=NULL,sram=NULL,vram==NULL;
}

rom::~rom()
{
  release();
}

void rom::reset()
{
}

void rom::release()
{
  if (rom_dat){
    delete []rom_dat;
    rom_dat=NULL;
  }
  if (chr_dat){
    delete []chr_dat;
    chr_dat=NULL;
  }
  if (sram){
    delete []sram;
    sram=NULL;
  }
  if (vram){
    delete []vram;
    vram=NULL;
  }
}

bool rom::load(const char *fname)
{
  release();

  FILE *f=fopen(fname,"rb");
  if (f==NULL) return false;

  char sig[4];
  fread(sig,1,4,f);
  if (strncmp(sig,"NES\x1A",4)!=0){
    fclose(f);
    return false;
  }

  unsigned char t,u;
  fread(&t,1,1,f);
  prg_page_cnt=t;

  fread(&t,1,1,f);
  chr_page_cnt=t;

  fread(&t,1,1,f);
  fread(&u,1,1,f);

  mirroring     =(t&1)?VERTICAL:HORIZONTAL;
  sram_enable   =(t&2)!=0;
  trainer_enable=(t&4)!=0;
  four_screen   =(t&8)!=0;
  mapper        =(t>>4)|(u&0xf0);

  char pad[8];
  fread(pad,1,8,f);

  int rom_size=0x4000*prg_page_cnt;
  int chr_size=0x2000*chr_page_cnt;
  rom_dat=new u8[rom_size];
  if (chr_size!=0) chr_dat=new u8[chr_size];
  sram=new u8[0x2000];
  vram=new u8[0x2000];

  fread(rom_dat,1,rom_size,f);
  fread(chr_dat,1,chr_size,f);

  fclose(f);

  cout<<"Cartridge information:"<<endl;
  cout<<(rom_size/1024)<<" KB rom, "<<(chr_size/1024)<<" KB vrom"<<endl;
  cout<<"mapper #"<<mapper<<endl;
  cout<<(mirroring==VERTICAL?"vertical":"horizontal")<<" mirroring"<<endl;
  cout<<"sram        : "<<(sram_enable?"Y":"N")<<endl;
  cout<<"trainer     : "<<(trainer_enable?"Y":"N")<<endl;
  cout<<"four screen : "<<(four_screen?"Y":"N")<<endl;
  cout<<endl;

  return true;
}

bool rom::save_sram(const char *fname)
{
  FILE *f=fopen(fname,"wb");
  if (f==NULL) return false;
  fwrite(sram,1,0x2000,f);
  fclose(f);
}

bool rom::load_sram(const char *fname)
{
  FILE *f=fopen(fname,"rb");
  if (f==NULL) return false;
  fseek(f,0,SEEK_END);
  int size=ftell(f);
  fseek(f,0,SEEK_SET);
  fread(sram,1,min(0x2000,size),f);
  fclose(f);
}
