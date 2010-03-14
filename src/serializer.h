#ifndef _SERIALIZER_H
#define _SERIALIZER_H

#include "types.h"
#include <map>
#include <vector>
#include <fstream>
#include <iostream>
using namespace std;

class chunk{
  friend class state_data;
public:
  enum mode{
    READ,WRITE
  };

  chunk(){
    cur_mode=WRITE;
    read_pos=0;
  }
  ~chunk(){
  }

  void set_mode(mode m) { cur_mode=m; }

  template <class T>
  chunk &operator<<(T &dat){
    if (cur_mode==READ) read(dat);
    else write(dat);
    return *this;
  }
  template <class T>
  chunk &operator<<(const T &dat){
    if (cur_mode==READ) throw "invalid operation";
    else write(dat);
    return *this;
  }

private:
  void load(ifstream &is){
    u32 size=(u8)is.get();
    size|=((u8)is.get())<<8;
    size|=((u8)is.get())<<16;
    size|=((u8)is.get())<<24;
    dat.resize(size);
    for (int i=0;i<size;i++)
      dat[i]=(u8)is.get();
  }
  void save(ofstream &os){
    u32 size=dat.size();
    os.put((u8)size);
    os.put((u8)(size>>8));
    os.put((u8)(size>>16));
    os.put((u8)(size>>24));
    for (int i=0;i<size;i++)
      os.put(dat[i]);
  }
  
  void write(const bool &d){
    dat.push_back(d?1:0);
  }
  void write(const u8 &d){
    dat.push_back(d);
  }
  void write(const u16 &d){
    dat.push_back((u8)d);
    dat.push_back((u8)(d>>8));
  }
  void write(const u32 &d){
    dat.push_back((u8)d);
    dat.push_back((u8)(d>>8));
    dat.push_back((u8)(d>>16));
    dat.push_back((u8)(d>>24));
  }
  void write(const s64 &d){
    dat.push_back((u8)d);
    dat.push_back((u8)(d>>8));
    dat.push_back((u8)(d>>16));
    dat.push_back((u8)(d>>24));
    dat.push_back((u8)(d>>32));
    dat.push_back((u8)(d>>40));
    dat.push_back((u8)(d>>48));
    dat.push_back((u8)(d>>56));
  }
  void write(const int &d) { write((const u32&)d); }

  void read(bool &d){
    d=readb()!=0;
  }
  void read(u8 &d){
    d=readb();
  }
  void read(u16 &d){
    d=readb();
    d|=readb()<<8;
  }
  void read(u32 &d){
    d=readb();
    d|=readb()<<8;
    d|=readb()<<16;
    d|=readb()<<24;
  }
  void read(s64 &d){
    d=readb();
    d|=(u64)(readb())<<8;
    d|=(u64)(readb())<<16;
    d|=(u64)(readb())<<24;
    d|=(u64)(readb())<<32;
    d|=(u64)(readb())<<40;
    d|=(u64)(readb())<<48;
    d|=(u64)(readb())<<56;
  }
  void read(int &d) { read((u32&)d); }

  u8 readb(){
    if (read_pos>=dat.size()) throw "out of bound";
    return dat[read_pos++];
  }
  
  vector<u8> dat;
  mode cur_mode;
  int read_pos;
};

class state_data{
public:
  state_data(ifstream &is){
    load(is);
    set_mode(chunk::READ);
  }
  state_data(){
  }
  ~state_data(){
  }

  void set_mode(chunk::mode m){
    for (map<string,chunk>::iterator p=dat.begin();
         p!=dat.end();p++)
      p->second.set_mode(m);
  }
  bool is_reading(){
    return !(dat.empty()||dat.begin()->second.cur_mode==chunk::WRITE);
  }

#define FILE_SIG "bjne save data 1.0.0"
  void load(ifstream &is){
    string sig=read_str(is);
    if (sig!=FILE_SIG) throw "invalid signature";
    for (;;){
      string tag=read_str(is);
      if (tag=="") break;
      dat[tag].load(is);
    }
  }
  void save(ofstream &os){
    os<<FILE_SIG<<'\0';
    for (map<string,chunk>::iterator p=dat.begin();
         p!=dat.end();p++){
      os<<p->first<<'\0';
      p->second.save(os);
    }
  }

  chunk &operator[](const string &s){
    return dat[s];
  }

private:
  string read_str(ifstream &is){
    string ret;
    char c;
    while(is.get(c)&&c!='\0') ret+=c;
    return ret;
  }

  map<string,chunk> dat;
};

#endif
