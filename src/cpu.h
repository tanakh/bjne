#ifndef _CPU_H
#define _CPU_H

#include "serializer.h"
class nes;
class mbc;

class cpu{
public:
  cpu(nes *n);
  ~cpu();

  void reset();
  void exec(int clk);

  void set_logging(bool b) { logging=b; }

  void set_nmi(bool b);
  void set_irq(bool b);
  void set_reset(bool b);

  s64 get_master_clock(); // エミュレーション開始からの累積クロックを返す。
  double get_frequency(); // 動作周波数を返す

  void serialize(state_data &sd);

private:
  u8  read8 (u16 adr);
  u16 read16(u16 adr);

  void write8 (u16 adr,u8  dat);
  void write16(u16 adr,u16 dat);

  enum IRQ_TYPE{
    NMI,IRQ,RESET,
  };
  void exec_irq(IRQ_TYPE);

  void log();

  u8 reg_a,reg_x,reg_y,reg_s;
  u16 reg_pc;
  u8 c_flag,z_flag,i_flag,d_flag,b_flag,v_flag,n_flag;

  int rest;
  s64 mclock;
  bool nmi_line,irq_line,reset_line;

  bool logging;
  
  nes *_nes;
  mbc *_mbc;
};

#endif
