#include "nes.h"
#include "mapper/null_mapper.h"
#include "mapper/mmc1.h"
#include "mapper/unrom.h"
#include "mapper/cnrom.h"
#include "mapper/mmc3.h"
#include "mapper/vrc2b.h"
#include "mapper/vrc4.h"
#include "mapper/vrc6.h"
#include "mapper/vrc6v.h"

mapper *mapper_maker::make_mapper(int num,nes *n)
{
  switch(num){
  case 0:  return new null_mapper(); // ‚È‚µ
  case 1:  return new mmc1(n);    // MMC1
  case 2:  return new unrom(n);   // UNROM
  case 3:  return new cnrom(n);   // CNROM
  case 4:  return new mmc3(n);    // MMC3
  case 23: return new vrc2b(n);   // VRC2 type B
  case 24: return new vrc6(n);    // VRC6
  case 25: return new vrc4(n);    // VRC4
  case 26: return new vrc6v(n);   // VRC6V

  default: return NULL;
  }
}

