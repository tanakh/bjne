//--------------------------------
// �O�E�Ƃ̃C���^�[�t�F�[�X

#ifndef _RENDERER_H
#define _RENDERER_H

#include <string>
using namespace std;

struct screen_info{
  void *buf;
  int width,height,pitch,bpp;
};

struct sound_info{
  void *buf;
  int freq,bps,ch,sample;
};

struct input_info{
  int *buf;
};

// �g����
// �܂� request_xxx ���Ăяo���B�K�v�ȃf�[�^���Ԃ��Ă���̂�
// �o�b�t�@�𖄂߂���Aoutput_xxx ���Ăяo���B
// request �̕Ԃ�l��NULL�������ꍇ�͏o�͂���K�v���Ȃ��B
// input �� output ����K�v�������B

class renderer{
public:
  virtual ~renderer(){}

  virtual screen_info *request_screen(int width,int height)=0;
  virtual sound_info  *request_sound()=0;
  virtual input_info  *request_input(int pad_count,int button_count)=0;

  virtual void output_screen(screen_info *info)=0;
  virtual void output_sound(sound_info *info)=0;

  virtual void output_message(const string &str)=0;
};

#endif
