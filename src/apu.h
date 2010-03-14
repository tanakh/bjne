#ifndef _APU_H
#define _APU_H

#include "renderer.h"
#include "serializer.h"
#include <queue>
using namespace std;

class nes;

class apu{
public:
  apu(nes *n);
  ~apu();

  void reset();

  u8 read(u16 adr);
  void write(u16 adr,u8 dat);

  void gen_audio(sound_info *info);
  void sync();

  void serialize(state_data &sd);

private:
  struct ch_state{
    bool enable; // チャンネルが有効か

    int wave_length; // 周波数

    bool length_enable; // 長さカウンタ有効
    int length; // 長さカウンタ
    double length_clk;

    int volume; // ボリューム
    int envelope_rate; // 減衰レート
    bool envelope_enable; // 減衰有効
    double envelope_clk; // 内部計算用

    bool sweep_enable; // スウィープ有効
    int sweep_rate; // スウィープリフレッシュレート
    bool sweep_mode; // スウィープ増減モード
    int sweep_shift; // スウィープシフト量
    double sweep_clk; // 内部計算用
    bool sweep_pausing;

    int duty; // デューティー比

    int linear_latch; // リニアカウンター用ラッチ
    int linear_counter; // リニアカウンター
    bool holdnote; // リニアカウンター動作中?
    int counter_start; // カウンタ開始
    double linear_clk;

    bool random_type; // 偽乱数モード(32K/93)

    int step; // 波形生成に使用
    double step_clk; // 同上
    int shift_register; // 偽乱数に使用
  } ch[4],sch[4];

  struct dmc_state{
    bool enable;
    bool irq;

    int playback_mode; // 再生モード
    int wave_length; // 周波数
    double clk; // クロック計算用

    int counter; // デルタカウンタ
    int length; // 長さ
    int length_latch;
    u16 adr; // アドレス
    u16 adr_latch;
    int shift_reg; // 内部シフトレジスタ
    int shift_count; // 同、カウンタ
    int dac_lsb; // DACのLSB
  } dmc,sdmc;

  enum ch_type{
    SQ1=0,SQ2=1,TRI=2,NOI=3,DMC=4,
  };

  void _write(ch_state *ch,dmc_state &dmc,u16 adr,u8 dat);

  // 波形を生成。すべて振幅は1の波で
  double sq_produce(ch_state &cc,double clk);
  double tri_produce(ch_state &cc,double clk);
  double noi_produce(ch_state &cc,double clk);
  double dmc_produce(double clk);
  
  struct write_dat{
    write_dat():clk(0),adr(0),dat(0) {}
    write_dat(s64 clk,u16 adr,u8 dat):clk(clk),adr(adr),dat(dat) {}

    s64 clk;
    u16 adr;
    u8 dat;
  };
  queue<write_dat> write_queue;
  s64 bef_clock;
  s64 bef_sync;
  
  nes *_nes;
};

#endif
