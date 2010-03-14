//--------------------------------
// 外界とのインターフェース

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

// 使い方
// まず request_xxx を呼び出す。必要なデータが返ってくるので
// バッファを埋めた後、output_xxx を呼び出す。
// request の返り値がNULLだった場合は出力する必要がない。
// input は output する必要が無い。

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
