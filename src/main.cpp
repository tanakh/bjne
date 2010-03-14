#include <SDL.h>
#include <iostream>
#include <ctime>
#include <string>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include "nes.h"
#include "sdl_renderer.h"
#include "timer.h"

using namespace std;

namespace fs=boost::filesystem;

string get_base_name(fs::path &p)
{
  string s=p.leaf();
  string::size_type pos=s.rfind(".");
  if (pos==string::npos)
    return s;
  else
    return s.substr(0,pos);
}

int main(int argc,char *argv[])
{
  freopen("CON","w",stdout);

  if (argc<2){
    cout<<"usage: "<<argv[0]<<" <romimage>"<<endl;
    return 0;
  }

  // ファイル周り
  fs::path rom_file(argv[1],fs::native);
  fs::path save_dir("save");
  fs::create_directory(save_dir);
  string save_base=get_base_name(rom_file);

  // 初期化
  cout<<"Initializing SDL renderer ..."<<endl;
  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER)<0){
    cout<<"failed to initialize SDL"<<endl;
    return -1;
  }

  SDL_Surface *sur=SDL_SetVideoMode(256,240,0,0);
  if (sur==NULL){
    cout<<"failed to set video mode"<<endl;
    SDL_Quit();
    return -1;
  }
  SDL_WM_SetCaption("NES Emulator","NES Emulator");

  renderer *nr=new sdl_renderer(sur);

  cout<<"Creating NES virtual machine ..."<<endl;
  nes *_nes=new nes(nr);
  cout<<"Loading ROM image ..."<<endl;
  cout<<endl;
  if (!_nes->load(rom_file.native_file_string().c_str())){
    cout<<"invalid rom-image"<<endl;
    goto _quit;
  }
  if (!_nes->check_mapper()){
    cout<<"unsupported mapper."<<endl;
    goto _quit;
  }
  {
    fs::path sram_path=(save_dir/(save_base+".sram"));
    if (fs::exists(sram_path)){
      cout<<"Loading SRAM ..."<<endl;
      if (!_nes->load_sram(sram_path.native_file_string().c_str()))
        cout<<"Fail to load SRAM ..."<<endl;
    }
  }
  //_nes->get_cpu()->set_logging(true);

  // 実行ループ
  {
    cout<<"Start emulation ..."<<endl;
    fps_timer timer(60);
    for(int cnt=0;;cnt++){
      SDL_Event event;
      while (SDL_PollEvent(&event)){
        switch(event.type){
        case SDL_QUIT:
          goto _quit;
        case SDL_KEYDOWN:
          {
            SDLKey ks=event.key.keysym.sym;
            if (ks==SDLK_t)
              _nes->get_cpu()->set_logging(true);
            else if (ks==SDLK_r)
              _nes->reset();
            else if (ks==SDLK_F1||ks==SDLK_F2||ks==SDLK_F3||ks==SDLK_F4||ks==SDLK_F5||
                     ks==SDLK_F6||ks==SDLK_F7||ks==SDLK_F8||ks==SDLK_F9){

              int n=(ks==SDLK_F1?1:ks==SDLK_F2?2:ks==SDLK_F3?3:ks==SDLK_F4?4:ks==SDLK_F5?5:
                     ks==SDLK_F6?6:ks==SDLK_F7?7:ks==SDLK_F8?8:ks==SDLK_F9?9:0);
              string s=(save_dir/(save_base+"-"+(char)('0'+n)+".state")).native_file_string();

              if (event.key.keysym.mod&KMOD_SHIFT){
                if (_nes->save_state(s.c_str()))
                  cout<<"Save State to #"<<n<<endl;
              }
              else{
                if (_nes->load_state(s.c_str()))
                  cout<<"Restore State from #"<<n<<endl;
                else
                  cout<<"Fail to Restore State #"<<n<<endl;
              }
            }
          }
          break;
        }
      }
      
      _nes->exec_frame();

      Uint8 *keys=SDL_GetKeyState(NULL);
      bool fast=keys[SDLK_TAB]==SDL_PRESSED;
      
      ((sdl_renderer*)nr)->skip_render(fast&&(cnt%10!=0));
      if (!fast)
        timer.elapse();
    }
  }

_quit:;
  {
    fs::path sram_path=(save_dir/(save_base+".sram"));
    if (_nes->save_sram(sram_path.native_file_string().c_str()))
      cout<<"Saving SRAM ..."<<endl;
  }

  delete nr;
  delete _nes;

  SDL_Quit();

  return 0;
}
