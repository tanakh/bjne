#include "nes.h"
#include <iostream>
using namespace std;

nes::nes(renderer *r) : _renderer(r)
{
  _cpu=new cpu(this);
  _apu=new apu(this);
  _ppu=new ppu(this);
  _mbc=new mbc(this);
  _regs=new regs(this);
  _rom=new rom();
  _mapper=NULL;
}

nes::~nes()
{
  delete _cpu;
  delete _apu;
  delete _ppu;
  delete _mbc;
  delete _regs;
  delete _rom;
  if (_mapper) delete _mapper;
}

bool nes::load(const char *fname)
{
  if (!_rom->load(fname)) return false;
  reset();
  return true;
}

bool nes::save_sram(const char *fname)
{
  if (_rom->has_sram())
    return _rom->save_sram(fname);
  else return false;
}

bool nes::load_sram(const char *fname)
{
  return _rom->load_sram(fname);
}

void nes::reset()
{
  // ROMとMBCはほかより先にリセットされていないといけない
  _rom->reset();
  _mbc->reset();

  // マッパー作成
  if (_mapper) delete _mapper;
  _mapper=mapper_maker().make_mapper(_rom->mapper_no(),this);

  // その他をリセット
  _cpu->reset();
  _apu->reset();
  _ppu->reset();
  _regs->reset();

  cout<<"Reset virtual machine ... "<<endl;
}

bool nes::check_mapper()
{
  return _mapper!=NULL;
}

bool nes::save_state(const char *fname)
{
  ofstream os(fname,ios::binary);
  state_data sd;

  _regs->serialize(sd);
  _cpu->serialize(sd);
  _ppu->serialize(sd);
  _apu->serialize(sd);
  _mbc->serialize(sd);
  if (_mapper!=NULL)
    _mapper->serialize(sd);

  sd.save(os);
}

bool nes::load_state(const char *fname)
{
  try{
    ifstream is(fname,ios::binary);
    state_data sd(is);
    
    _regs->serialize(sd);
    _cpu->serialize(sd);
    _ppu->serialize(sd);
    _apu->serialize(sd);
    _mbc->serialize(sd);
    if (_mapper!=NULL)
      _mapper->serialize(sd);
    return true;
  }
  catch(...){
    cout<<"invalid state file..."<<endl;
    return false;
  }
}

void nes::exec_frame()
{
  // CPUクロックは1.7897725MHzとのこと。
  // 1789772.5 / 60 / 262 = 113.85...
  // 1 line = 114 clock ?
  // 1789772.5 / 262 / 114 = 59.922 fps ?

  screen_info *scri=_renderer->request_screen(256,240);
  sound_info  *sndi=_renderer->request_sound();
  input_info  *inpi=_renderer->request_input(2,8);

  if (sndi){
    _apu->gen_audio(sndi);
    _renderer->output_sound(sndi);
  }
  if (inpi)
    _regs->set_input(inpi->buf);
  
  _regs->set_vblank(false,true);
  _regs->start_frame();
  for (int i=0;i<240;i++){
    if (_mapper) _mapper->hblank(i);
    _regs->start_scanline();
    if (scri) _ppu->render(i,scri);
    _ppu->sprite_check(i);
    _apu->sync();
    _cpu->exec(114);
    _regs->end_scanline();
  }

  if ((_regs->frame_irq&0xC0)==0)
    _cpu->set_irq(true);

  for (int i=240;i<262;i++){
    if (_mapper) _mapper->hblank(i);
    _apu->sync();
    if (i==241){
      _regs->set_vblank(true,false);
      _cpu->exec(0); // VBLANK突入後、NMI発生までに1命令ぐらい実行される。
      _regs->set_vblank(true,true);
      _cpu->exec(114);
    }
    else
      _cpu->exec(114);
  }

  if (scri)
    _renderer->output_screen(scri);
}
