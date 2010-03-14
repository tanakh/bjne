#include "nes.h"
#include <memory.h>
#include <algorithm>
#include <iostream>
#include <iomanip>
using namespace std;

apu::apu(nes *n) :_nes(n)
{
}

apu::~apu()
{
}

void apu::reset()
{
  memset(&ch,0,sizeof(ch));
  memset(&dmc,0,sizeof(dmc));
  ch[3].shift_register=1;

  bef_clock=bef_sync=0;
}

u8 apu::read(u16 adr)
{
  if (adr==0x4015){
    sync();
    return (sch[0].length==0?0:1)
      |((sch[1].length==0?0:1)<<1)
      |((sch[2].length==0?0:1)<<2)
      |((sch[3].length==0?0:1)<<3)
      |((sdmc.enable?1:0)<<4)
      |((sdmc.irq?1:0)<<7);
  }
  return 0xA0; // 4015以外は読み取れないが、データバスのキャパシタンスの関係で0xA0が読み込めるらしい
}

void apu::write(u16 adr,u8 dat)
{
  // サウンド生成の都合上書き込みは遅延させる。
  write_queue.push(write_dat(_nes->get_cpu()->get_master_clock(),adr,dat));
  while(write_queue.size()>100){ // あんまりキューが詰まると実行に支障をきたすので…
    write_dat wd=write_queue.front();
    write_queue.pop();
    _write(ch,dmc,wd.adr,wd.dat);
  }
  // ステータスレジスタのための処理
  sync();
  _write(sch,sdmc,adr,dat);
}

void apu::_write(ch_state *ch,dmc_state &dmc,u16 adr,u8 dat)
{
  //if (adr>=0x4010)
  //  cout<<hex<<setw(4)<<setfill('0')<<adr<<" <- "<<setw(2)<<(int)dat<<endl;
  
  // 実際にはこっちで書き込む
  int cn=(adr&0x1f)/4;
  ch_state &cc=ch[cn];

  switch(adr){
  case 0x4000: case 0x4004: case 0x400C:
    cc.envelope_enable=(dat&0x10)==0;
    if (cc.envelope_enable){
      cc.volume=0xf;
      cc.envelope_rate=dat&0xf;
    }
    else
      cc.volume=dat&0xf;
    cc.length_enable=(dat&0x20)==0;
    cc.duty=dat>>6;
    cc.envelope_clk=0;
    break;
  case 0x4008:
    cc.linear_latch=dat&0x7f;
    cc.holdnote=(dat&0x80)!=0;
    break;

  case 0x4001: case 0x4005:
    cc.sweep_shift=dat&7;
    cc.sweep_mode=(dat&0x8)!=0;
    cc.sweep_rate=(dat>>4)&7;
    cc.sweep_enable=(dat&0x80)!=0;
    cc.sweep_clk=0;
    cc.sweep_pausing=false;
    break;
  case 0x4009: case 0x400D: // unused
    break;

  case 0x4002: case 0x4006: case 0x400A:
    cc.wave_length=(cc.wave_length&~0xff)|dat;
    break;
  case 0x400E: {
    const static int conv_table[16]={
      0x002,0x004,0x008,0x010,
      0x020,0x030,0x040,0x050,
      0x065,0x07F,0x0BE,0x0FE,
      0x17D,0x1FC,0x3F9,0x7F2,
    };
    cc.wave_length=conv_table[dat&0xf]-1;
    cc.random_type=(dat&0x80)!=0;
    break;
  }
  case 0x4003: case 0x4007: case 0x400B: case 0x400F: {
    const static int length_tbl[16]={
      0x05,0x06,0x0A,0x0C,
      0x14,0x18,0x28,0x30,
      0x50,0x60,0x1E,0x24,
      0x07,0x08,0x0E,0x10,
    };
    if (cn!=3) cc.wave_length=(cc.wave_length&0xff)|((dat&0x7)<<8);
    if ((dat&0x8)==0)
      cc.length=length_tbl[dat>>4];
    else
      cc.length=(dat>>4)==0?0x7f:(dat>>4);
    if (cn==2)
      cc.counter_start=true;

    if (cc.envelope_enable){
      cc.volume=0xf;
      cc.envelope_clk=0;
    }
    break;
  }

  case 0x4010: {
    const static int dac_table[16]={
      0xD60,0xBE0,0xAA0,0xA00,
      0x8F0,0x7F0,0x710,0x6B0,
      0x5F0,0x500,0x470,0x400,
      0x350,0x2A0,0x240,0x1B0,
    };
    dmc.playback_mode=dat>>6;
    dmc.wave_length=dac_table[dat&0xf]/8;
    if ((dat>>7)==0)
      dmc.irq=false;
    break;
  }
  case 0x4011:
    dmc.dac_lsb=dat&1;
    dmc.counter=(dat>>1)&0x3f;
    break;
  case 0x4012:
    dmc.adr_latch=(dat<<6)|0xC000;
    break;
  case 0x4013:
    dmc.length_latch=(dat<<4)+1;
    break;

  case 0x4015:
    ch[0].enable=(dat&1)!=0;
    if (!ch[0].enable) ch[0].length=0;
    ch[1].enable=(dat&2)!=0;
    if (!ch[1].enable) ch[1].length=0;
    ch[2].enable=(dat&4)!=0;
    if (!ch[2].enable) ch[2].length=0;
    ch[3].enable=(dat&8)!=0;
    if (!ch[3].enable) ch[3].length=0;

    if ((dat&0x10)!=0){
      if (!dmc.enable){
        dmc.adr=dmc.adr_latch;
        dmc.length=dmc.length_latch;
        dmc.shift_count=0;
      }
      dmc.enable=true;
    }
    else
      dmc.enable=false;
    dmc.irq=false;
    break;
  }
}

double apu::sq_produce(ch_state &cc,double clk)
{
  static const int sq_wav[4][16]={
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1},
    {0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1},
    {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1},
  };
  #define sqwav_dat (0.5-sq_wav[cc.duty][cc.step])

  cc.step_clk+=clk;
  double ret=sqwav_dat;
  double term=cc.wave_length+1;
  if (cc.step_clk>=term){
    int t=(int)(cc.step_clk/term);
    cc.step_clk-=term*t;
    cc.step=(cc.step+t)%16;
  }
  return ret;
}

double apu::tri_produce(ch_state &cc,double clk)
{
  static const int tri_wav[32]={
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,
  };
  #define triwav_dat (tri_wav[cc.step]/16.0-0.5)

  cc.step_clk+=clk;
  double ret=triwav_dat;
  double term=cc.wave_length+1;
  if (cc.step_clk>=term){
    int t=(int)(cc.step_clk/term);
    cc.step_clk-=term*t;
    cc.step=(cc.step+t)%32;
  }
  return ret;
}

double apu::noi_produce(ch_state &cc,double clk)
{
  cc.step_clk+=clk;
  double ret=0.5-(cc.shift_register>>14);
  double term=cc.wave_length+1;

  while (cc.step_clk>=term){
    cc.step_clk-=term;
    int t=cc.shift_register;
    if (cc.random_type)
      cc.shift_register=((t<<1)|(((t>>14)^(t>>8))&1))&0x7fff;
    else
      cc.shift_register=((t<<1)|(((t>>14)^(t>>13))&1))&0x7fff;
  }
  return ret;
}

double apu::dmc_produce(double clk)
{
#define retval ((((dmc.counter<<1)|dmc.dac_lsb)-64)/32.0)

  if (!dmc.enable) return retval;

  dmc.clk+=clk;
  while (dmc.clk>dmc.wave_length){
    dmc.clk-=dmc.wave_length;
    if (dmc.shift_count==0){
      if (dmc.length==0){ // 終わってますか
        if ((dmc.playback_mode&1)!=0){ // ループモード
          dmc.adr=dmc.adr_latch;
          dmc.length=dmc.length_latch;
        }
        else{
          dmc.enable=false;
          if (dmc.playback_mode==2){ // IRQ発生
            dmc.irq=true;
            // _nes->get_cpu()->set_irq(true); // 実際の割り込みはsyncで起こす
          }
          return retval;
        }
      }
      dmc.shift_count=8;
      dmc.shift_reg=_nes->get_mbc()->read(dmc.adr);
      //cout<<"dmc read : "<<hex<<setw(4)<<setfill('0')<<dmc.adr<<endl;
      if (dmc.adr==0xFFFF) dmc.adr=0x8000;
      else dmc.adr++;
      dmc.length--;
    }
    
    int b=dmc.shift_reg&1;
    if (b==0&&dmc.counter!=0) // decrement
      dmc.counter--;
    if (b==1&&dmc.counter!=0x3F)
      dmc.counter++;
    dmc.counter&=0x3f;
    dmc.shift_count--;
    dmc.shift_reg>>=1;
  }
  return retval;
}

void apu::gen_audio(sound_info *info)
{
  static const double cpu_clock=_nes->get_cpu()->get_frequency();
  static const double framerate=(cpu_clock*2/14915);
  
  s64 cur_clock=_nes->get_cpu()->get_master_clock();
  int sample=info->sample;

  double inc_clk=((double)(cur_clock-bef_clock))/sample; // 1サンプルあたりの実行CPUクロック
  double sample_clk=cpu_clock/info->freq; // 1サンプルあたりのCPUクロック

  memset(info->buf,0,info->bps/8*info->sample*info->ch);
  if (_nes->get_mapper()) // 拡張音源
    _nes->get_mapper()->audio(info);

  /*
  cout<<"sq1 "<<ch[0].wave_length<<" "<<ch[0].length<<" "<<ch[0].volume<<endl;
  cout<<"sq2 "<<ch[1].wave_length<<" "<<ch[1].length<<" "<<ch[1].volume<<endl;
  cout<<"tri "<<ch[2].wave_length<<" "<<ch[2].length<<" "<<ch[2].volume<<endl;
  cout<<"noi "<<ch[3].wave_length<<" "<<ch[3].length<<" "<<ch[3].volume<<endl;
   */

  for (int i=0;i<sample;i++){
    s64 pos=(cur_clock-bef_clock)*i/sample+bef_clock;
    while(!write_queue.empty()&&
          write_queue.front().clk<=pos){
      write_dat wd=write_queue.front();
      write_queue.pop();
      _write(ch,dmc,wd.adr,wd.dat);
    }

    double v=0;

    for (int j=0;j<4;j++){
      ch_state &cc=ch[j];

      bool pause=false;
      if (!cc.enable) continue;
      if (cc.length==0) pause=true;

      // length counter
      if (cc.length_enable){
        double length_clk=cpu_clock/60.0;
        cc.length_clk+=inc_clk;
        while(cc.length_clk>length_clk){
          cc.length_clk-=length_clk;
          if (cc.length>0) cc.length--;
        }
      }
      // linear counter
      if (j==TRI){
        if (cc.counter_start)
          cc.linear_counter=cc.linear_latch;
        else{
          double linear_clk=cpu_clock/240.0;
          cc.linear_clk+=inc_clk;
          while(cc.linear_clk>linear_clk){
            cc.linear_clk-=linear_clk;
            if (cc.linear_counter>0) cc.linear_counter--;
          }
        }
        if (!cc.holdnote&&cc.linear_counter!=0)
          cc.counter_start=false;
        
        if (cc.linear_counter==0) pause=true;
      }

      // エンベロープ
      int vol=16;
      if (j!=TRI){
        if (cc.envelope_enable){
          double decay_clk=cpu_clock/(240.0/(cc.envelope_rate+1));
          cc.envelope_clk+=inc_clk;
          while (cc.envelope_clk>decay_clk){
            cc.envelope_clk-=decay_clk;
            if (cc.volume>0) cc.volume--;
            else{
              if (!cc.length_enable) // ループ
                cc.volume=0xf;
              else
                cc.volume=0;
            }
          }
        }
        vol=cc.volume;
      }

      // スウィープ
      if ((j==SQ1||j==SQ2)&&cc.sweep_enable&&!cc.sweep_pausing){
        double sweep_clk=cpu_clock/(120.0/(cc.sweep_rate+1));
        cc.sweep_clk+=inc_clk;
        while (cc.sweep_clk>sweep_clk){
          cc.sweep_clk-=sweep_clk;
          if (cc.sweep_shift!=0&&cc.length!=0){
            if (!cc.sweep_mode) // increase
              cc.wave_length+=cc.wave_length>>cc.sweep_shift;
            else // decrease
              cc.wave_length+=~(cc.wave_length>>cc.sweep_shift); // 1の補数
            if (cc.wave_length<0x008) cc.sweep_pausing=true;
            if (cc.wave_length&~0x7FF) cc.sweep_pausing=true;
            cc.wave_length&=0x7FF;
          }
        }
      }

      pause|=cc.sweep_pausing;
      pause|=cc.wave_length==0;
      if (pause) continue;

      // 波形生成
      double t=((j==SQ1||j==SQ2)?sq_produce(cc,sample_clk):
             (j==TRI)?tri_produce(cc,sample_clk):
             (j==NOI)?noi_produce(cc,sample_clk):0);

      v+=t*vol/16;
    }

    v+=dmc_produce(sample_clk);

    // 出力
    if (info->bps==16)
      ((s16*)info->buf)[i]=(s16)min(32767.0,max(-32767.0,(((s16*)info->buf)[i]+v*8000)));
    else if (info->bps==8)
      ((u8*)info->buf)[i]+=(u8)(v*30);
  }
  bef_clock=cur_clock;
}

void apu::sync()
{
  double cpu_clock=_nes->get_cpu()->get_frequency();
  s64 cur=_nes->get_cpu()->get_master_clock();
  int adv_clock=(int)cur-bef_sync;

  // 4つのチャンネルの更新(長さカウンタだけ処理しとけばよろしい)
  for (int j=0;j<4;j++){
    ch_state &cc=sch[j];
    // length counter
    if (cc.enable&&cc.length_enable){
      double length_clk=cpu_clock/60.0;
      cc.length_clk+=adv_clock;
      int dec=(int)(cc.length_clk/length_clk);
      cc.length_clk-=length_clk*dec;
      cc.length=max(0,cc.length-dec);
    }
  }
  // DMCの更新
  if (sdmc.enable){
    sdmc.clk+=adv_clock;
    int dec=(int)(sdmc.clk/sdmc.wave_length);
    sdmc.clk-=dec*sdmc.wave_length;

    int rest=sdmc.shift_count+sdmc.length*8-dec;
    if (rest<=0){ // 再生が終わる
      if ((sdmc.playback_mode&1)!=0){ // ループする
        sdmc.length=rest/8;
        while(sdmc.length<0) sdmc.length+=sdmc.length_latch;
        sdmc.shift_count=0;
      }
      else{
        sdmc.enable=false;
        if (sdmc.playback_mode==2){ // IRQ発生
          sdmc.irq=true;
          _nes->get_cpu()->set_irq(true);
        }
      }
    }
    else{
      sdmc.length=rest/8;
      sdmc.shift_count=rest%8;
    }
  }

  bef_sync=cur;
}

void apu::serialize(state_data &sd)
{
  sync();
  while(!write_queue.empty()){
    write_dat wd=write_queue.front();
    write_queue.pop();
    _write(ch,dmc,wd.adr,wd.dat);
  }
  sd["APU"]<<bef_clock<<bef_sync;

  for (int i=0;i<2;i++){
    ch_state *p=i==0?ch:sch;
    for (int j=0;j<4;j++){
      ch_state &cc=p[j];
      sd["APU"]<<cc.enable<<cc.wave_length
        <<cc.length_enable<<cc.length
        <<cc.volume<<cc.envelope_rate<<cc.envelope_enable
        <<cc.sweep_enable<<cc.sweep_rate<<cc.sweep_mode<<cc.sweep_shift<<cc.sweep_pausing
        <<cc.duty<<cc.linear_latch<<cc.linear_counter<<cc.holdnote<<cc.counter_start
        <<cc.random_type<<cc.step<<cc.shift_register;
    }
  }

  for (int i=0;i<2;i++){
    dmc_state &dc=i==0?dmc:sdmc;
    sd["APU"]<<dc.enable<<dc.irq
      <<dc.playback_mode<<dc.wave_length
      <<dc.counter<<dc.length<<dc.length_latch
      <<dc.adr<<dc.adr_latch<<dc.shift_reg<<dc.shift_count<<dc.dac_lsb;
  }
}
