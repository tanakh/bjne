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
    bool enable; // �`�����l�����L����

    int wave_length; // ���g��

    bool length_enable; // �����J�E���^�L��
    int length; // �����J�E���^
    double length_clk;

    int volume; // �{�����[��
    int envelope_rate; // �������[�g
    bool envelope_enable; // �����L��
    double envelope_clk; // �����v�Z�p

    bool sweep_enable; // �X�E�B�[�v�L��
    int sweep_rate; // �X�E�B�[�v���t���b�V�����[�g
    bool sweep_mode; // �X�E�B�[�v�������[�h
    int sweep_shift; // �X�E�B�[�v�V�t�g��
    double sweep_clk; // �����v�Z�p
    bool sweep_pausing;

    int duty; // �f���[�e�B�[��

    int linear_latch; // ���j�A�J�E���^�[�p���b�`
    int linear_counter; // ���j�A�J�E���^�[
    bool holdnote; // ���j�A�J�E���^�[���쒆?
    int counter_start; // �J�E���^�J�n
    double linear_clk;

    bool random_type; // �U�������[�h(32K/93)

    int step; // �g�`�����Ɏg�p
    double step_clk; // ����
    int shift_register; // �U�����Ɏg�p
  } ch[4],sch[4];

  struct dmc_state{
    bool enable;
    bool irq;

    int playback_mode; // �Đ����[�h
    int wave_length; // ���g��
    double clk; // �N���b�N�v�Z�p

    int counter; // �f���^�J�E���^
    int length; // ����
    int length_latch;
    u16 adr; // �A�h���X
    u16 adr_latch;
    int shift_reg; // �����V�t�g���W�X�^
    int shift_count; // ���A�J�E���^
    int dac_lsb; // DAC��LSB
  } dmc,sdmc;

  enum ch_type{
    SQ1=0,SQ2=1,TRI=2,NOI=3,DMC=4,
  };

  void _write(ch_state *ch,dmc_state &dmc,u16 adr,u8 dat);

  // �g�`�𐶐��B���ׂĐU����1�̔g��
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
