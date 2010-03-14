#ifndef _MAPPER_H
#define _MAPPER_H

#include "types.h"
#include "renderer.h"
#include "serializer.h"

class mapper{
public:
  virtual ~mapper() {}
  virtual void write(u16 adr,u8 dat) {}
  virtual void hblank(int line) {}
  virtual void audio(sound_info *info) {}
  virtual void serialize(state_data &sd) {}
};

class mapper_maker{
public:
  mapper *make_mapper(int num,nes *n);
};

#endif
