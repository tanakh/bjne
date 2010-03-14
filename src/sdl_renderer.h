#include <SDL.h>
#include <memory.h>
#include <iostream>
using namespace std;

#define BUF_SIZE 2048
#define RBUF_SIZE (BUF_SIZE*2)

class sdl_renderer : public renderer {
public:
  sdl_renderer(SDL_Surface *bg){
    this->bg=bg;
    ren=NULL;
    bef_width=bef_height=0;

    skip=false;

    audio_init();
  }
  ~sdl_renderer(){
    release_surface();
    SDL_CloseAudio();
  }

  void skip_render(bool b){
    skip=b;
  }
  
  virtual screen_info *request_screen(int width,int height){
    if (skip)
      return NULL;
    
    if (bef_width!=width||bef_height!=height||ren==NULL){
      bef_width=width,bef_height=height;
      release_surface();
      ren=SDL_AllocSurface(SDL_SWSURFACE,width,height,32,0xff,0xff00,0xff0000,0x00000000);
    }

    SDL_LockSurface(ren);
    static screen_info ret;

    ret.width=width;
    ret.height=height;
    ret.pitch=ren->pitch;
    ret.bpp=32; // あとで変える。
    ret.buf=ren->pixels;

    return &ret;
  }
  virtual sound_info *request_sound(){
    static sound_info ret;
    static Uint16 tmp_buf[RBUF_SIZE];
    if (dat==play)
      return NULL;

    int sample=((dat+RBUF_SIZE)-play)%RBUF_SIZE;
    ret.freq=44100;
    ret.bps=16;
    ret.ch=1;
    ret.sample=sample==0?RBUF_SIZE:sample;
    ret.buf=tmp_buf;
    return &ret;
  }
  virtual input_info *request_input(int pad_count,int button_count){
    static int buf[256];
    static input_info ret={buf};

    // キー定義。とりあえずべたべたと。
    // A B セレクト スタート 上 下 左 右
    int key_def[2][8]={
      {SDLK_z,SDLK_x,SDLK_RSHIFT,SDLK_RETURN,SDLK_UP,SDLK_DOWN,SDLK_LEFT,SDLK_RIGHT},
      {SDLK_v,SDLK_b,SDLK_n,SDLK_m,SDLK_0,SDLK_COMMA,SDLK_k,SDLK_l}
    };

    Uint8 *keys=SDL_GetKeyState(NULL);
    int *p=buf;
    for (int i=0;i<pad_count;i++)
      for (int j=0;j<button_count;j++)
        *(p++)=keys[key_def[i][j]]==SDL_PRESSED?1:0;

    return &ret;
  }

  virtual void output_screen(screen_info *info){
    SDL_UnlockSurface(ren);
    SDL_BlitSurface(ren,NULL,bg,NULL);
    SDL_Flip(bg);
  }

  virtual void output_sound(sound_info *info){
    Uint16 *src=(Uint16*)info->buf;
    for (int i=0;i<info->sample;i++)
      buf[(dat+i)%RBUF_SIZE]=src[i];
    dat=(dat+info->sample)%RBUF_SIZE; // 空きを全部埋めた。
  }

  virtual void output_message(const string &str){
    cout<<str<<endl;
  }

private:
  void release_surface(){
    if (ren){
      SDL_FreeSurface(ren);
      ren=NULL;
    }
  }

  void audio_init(){
    dat=dat=0;
    memset(buf,0,RBUF_SIZE*sizeof(Uint16));
    
    SDL_AudioSpec fmt;
    fmt.freq=44100;
    fmt.format=AUDIO_S16;
    fmt.channels=1;
    fmt.samples=BUF_SIZE;
    fmt.callback=_mix_audio;
    fmt.userdata=(void*)this;
    SDL_OpenAudio(&fmt,NULL);
    SDL_PauseAudio(0);
  }

  static void _mix_audio(void *tp,Uint8 *stream,int len){
    ((sdl_renderer*)tp)->mix_audio(stream,len);
  }

  void mix_audio(Uint8 *stream,int len){
    memcpy(stream,buf+play,len);
    play=(play+len/2)%RBUF_SIZE; // lenは2の倍数であってほしい。
  }

  SDL_Surface *bg,*ren;
  int bef_width,bef_height;

  Uint16 buf[RBUF_SIZE]; // リングバッファ
  int dat,play;

  bool skip;
};
