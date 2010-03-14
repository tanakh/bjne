#include "nes.h"
#include <iostream>
#include <iomanip>
#include <sstream>
using namespace std;

cpu::cpu(nes *n) :_nes(n)
{
  logging=false;
}

cpu::~cpu()
{
}

void cpu::reset()
{
  _mbc=_nes->get_mbc();

  reg_pc=read16(0xFFFC);
  reg_a=0x00;
  reg_x=0x00;
  reg_y=0x00;
  reg_s=0xFF;

  n_flag=v_flag=z_flag=c_flag=0; // 不定だけど再現性のためにリセットしておく
  b_flag=1;
  d_flag=0;
  i_flag=1;

  rest=0;
  mclock=0;
  nmi_line=irq_line=reset_line=false;
}

void cpu::set_nmi(bool b)
{
  // エッジセンシティブな割り込み
  if (!nmi_line&&b)
    exec_irq(NMI);
  nmi_line=b;
}

void cpu::set_irq(bool b)
{
  // おそらくレベルセンシティブな割り込み
  // 処理の簡便のために、命令フェッチ前に割り込みチェックをする
  irq_line=b;
}

void cpu::set_reset(bool b)
{
  // 同上
  reset_line=b;
}

u8 cpu::read8(u16 adr)
{
  return _mbc->read(adr);
}

u16 cpu::read16(u16 adr)
{
  return read8(adr)|(read8(adr+1)<<8);
}

void cpu::write8(u16 adr,u8 dat)
{
  _mbc->write(adr,dat);
}

void cpu::write16(u16 adr,u16 dat)
{
  write8(adr,(u8)dat);
  write8(adr+1,(u8)(dat>>8));
}

#define _imm() (reg_pc++)

// TODO 桁上がりのペナルティー
#define _abs()  (reg_pc+=2,read16(opr_pc))
#define _abxi() (reg_pc+=2,read16(read16(opr_pc)+reg_x))
#define _abx()  (reg_pc+=2,read16(opr_pc)+reg_x)
#define _aby()  (reg_pc+=2,read16(opr_pc)+reg_y)
#define _absi() (reg_pc+=2,read16(read16(opr_pc)))

#define _zp()   (read8(reg_pc++))
#define _zpxi() (read16((u8)(read8(reg_pc++)+reg_x)))
#define _zpx()  ((u8)(read8(reg_pc++)+reg_x))
#define _zpy()  ((u8)(read8(reg_pc++)+reg_y))
#define _zpi()  (read16(read8(reg_pc++)))
#define _zpiy() (read16(read8(reg_pc++))+reg_y)

#define _push8(dat)  write8(0x100|(u8)(reg_s--),dat)
#define _pop8()      read8(0x100|(u8)(++reg_s))
#define _push16(dat) (write16(0x100|(u8)(reg_s-1),dat),reg_s-=2)
#define _pop16()     (reg_s+=2,read16(0x100|(u8)(reg_s-1)))

#define _bind_flags() ((n_flag<<7)|(v_flag<<6)|0x20|(b_flag<<4)|(d_flag<<3)|(i_flag<<2)|(z_flag<<1)|c_flag)
#define _unbind_flags(dd) { \
  u8 dat=dd; \
  n_flag=dat>>7; \
  v_flag=(dat>>6)&1; \
  b_flag=(dat>>4)&1; \
  d_flag=(dat>>3)&1; \
  i_flag=(dat>>2)&1; /* (iがクリアされた場合、割り込まれる可能性が) */ \
  z_flag=(dat>>1)&1; \
  c_flag=dat&1; \
}

// TODO : decimal support
#define _adc(cycle,adr) { \
  u8  s=read8(adr); \
  u16 t=reg_a+s+c_flag; \
  c_flag=(u8)(t>>8); \
  z_flag=(t&0xff)==0; \
  n_flag=(t>>7)&1; \
  v_flag=!((reg_a^s)&0x80)&&((reg_a^t)&0x80); \
  reg_a=(u8)t; \
  rest-=cycle; \
}
// TODO : decimal support
#define _sbc(cycle,adr) { \
  u8  s=read8(adr); \
  u16 t=reg_a-s-(c_flag?0:1); \
  c_flag=t<0x100; \
  z_flag=(t&0xff)==0; \
  n_flag=(t>>7)&1; \
  v_flag=((reg_a^s)&0x80)&&((reg_a^t)&0x80); \
  reg_a=(u8)t; \
  rest-=cycle; \
}
#define _cmp(cycle,reg,adr) { \
  u16 t=reg-read8(adr); \
  c_flag=t<0x100; \
  z_flag=(t&0xff)==0; \
  n_flag=(t>>7)&1; \
  rest-=cycle; \
}

#define _and(cycle,adr) { \
  reg_a&=read8(adr); \
  n_flag=reg_a>>7; \
  z_flag=reg_a==0; \
  rest-=cycle; \
}
#define _ora(cycle,adr) { \
  reg_a|=read8(adr); \
  n_flag=reg_a>>7; \
  z_flag=reg_a==0; \
  rest-=cycle; \
}
#define _eor(cycle,adr) { \
  reg_a^=read8(adr); \
  n_flag=reg_a>>7; \
  z_flag=reg_a==0; \
  rest-=cycle; \
}

#define _bit(cycle,adr) { \
  u8 t=read8(adr); \
  n_flag=t>>7; \
  v_flag=(t>>6)&1; \
  z_flag=(reg_a&t)==0; \
  rest-=cycle; \
}

#define _load(cycle,reg,adr) { \
  reg=read8(adr); \
  n_flag=reg>>7; \
  z_flag=reg==0; \
  rest-=cycle; \
}
#define _store(cycle,reg,adr) { \
  write8(adr,reg); \
  rest-=cycle; \
}

#define _mov(cycle,dest,src) { \
  dest=src; \
  n_flag=src>>7; \
  z_flag=src==0; \
  rest-=cycle; \
}

#define _asli(arg) \
  c_flag=arg>>7; \
  arg<<=1; \
  n_flag=arg>>7; \
  z_flag=arg==0;
#define _lsri(arg) \
  c_flag=arg&1; \
  arg>>=1; \
  n_flag=arg>>7; \
  z_flag=arg==0;
#define _roli(arg) \
  u8 u=arg; \
  arg=(arg<<1)|c_flag; \
  c_flag=u>>7; \
  n_flag=arg>>7; \
  z_flag=arg==0;
#define _rori(arg) \
  u8 u=arg; \
  arg=(arg>>1)|(c_flag<<7); \
  c_flag=u&1; \
  n_flag=arg>>7; \
  z_flag=arg==0;
#define _inci(arg) \
  arg++; \
  n_flag=arg>>7; \
  z_flag=arg==0;
#define _deci(arg) \
  arg--; \
  n_flag=arg>>7; \
  z_flag=arg==0;

#define _sfta(cycle,reg,op) { op(reg);rest-=cycle; }
#define _sft(cycle,adr,op) { \
  u16 a=adr; \
  u8 t=read8(a); \
  op(t); \
  write8(a,t); \
  rest-=cycle; \
}

#define _asla(cycle)    _sfta(cycle,reg_a,_asli)
#define _asl(cycle,adr) _sft(cycle,adr,_asli)
#define _lsra(cycle)    _sfta(cycle,reg_a,_lsri)
#define _lsr(cycle,adr) _sft(cycle,adr,_lsri)
#define _rola(cycle)    _sfta(cycle,reg_a,_roli)
#define _rol(cycle,adr) _sft(cycle,adr,_roli)
#define _rora(cycle)    _sfta(cycle,reg_a,_rori)
#define _ror(cycle,adr) _sft(cycle,adr,_rori)

#define _incr(cycle,reg) _sfta(cycle,reg,_inci)
#define _inc(cycle,adr)  _sft(cycle,adr,_inci)
#define _decr(cycle,reg) _sfta(cycle,reg,_deci)
#define _dec(cycle,adr)  _sft(cycle,adr,_deci)

#define _bra(cycle,cond) { \
  s8 rel=(s8)read8(_imm()); \
  rest-=cycle; \
  if (cond){ \
    rest-=(reg_pc&0xff00)==((reg_pc+rel)&0xff)?1:2; \
    reg_pc+=rel; \
  } \
}

// clkクロック実行(最低一命令は実行する)
void cpu::exec(int clk)
{
  rest+=clk;
  mclock+=clk;
  do{
    if (!i_flag){ // IRQ チェック
      // 割り込みがかかるとラインのレベルを落とす。本当は違うんだろうけど。
      if (reset_line) exec_irq(RESET),reset_line=false;
      else if (irq_line) exec_irq(IRQ),irq_line=false;
    }
    
    if (logging) log();

    u8  opc=read8(reg_pc++);
    u16 opr_pc=reg_pc;
    
    switch(opc){
      /* ALU */
    case 0x69: _adc(2,_imm());  break;
    case 0x65: _adc(3,_zp());   break;
    case 0x75: _adc(4,_zpx());  break;
    case 0x6D: _adc(4,_abs());  break;
    case 0x7D: _adc(4,_abx());  break;
    case 0x79: _adc(4,_aby());  break;
    case 0x61: _adc(6,_zpxi()); break;
    case 0x71: _adc(5,_zpiy()); break;

    case 0xE9: _sbc(2,_imm());  break;
    case 0xE5: _sbc(3,_zp());   break;
    case 0xF5: _sbc(4,_zpx());  break;
    case 0xED: _sbc(4,_abs());  break;
    case 0xFD: _sbc(4,_abx());  break;
    case 0xF9: _sbc(4,_aby());  break;
    case 0xE1: _sbc(6,_zpxi()); break;
    case 0xF1: _sbc(5,_zpiy()); break;

    case 0xC9: _cmp(2,reg_a,_imm());  break;
    case 0xC5: _cmp(3,reg_a,_zp());   break;
    case 0xD5: _cmp(4,reg_a,_zpx());  break;
    case 0xCD: _cmp(4,reg_a,_abs());  break;
    case 0xDD: _cmp(4,reg_a,_abx());  break;
    case 0xD9: _cmp(4,reg_a,_aby());  break;
    case 0xC1: _cmp(6,reg_a,_zpxi()); break;
    case 0xD1: _cmp(5,reg_a,_zpiy()); break;

    case 0xE0: _cmp(2,reg_x,_imm()); break;
    case 0xE4: _cmp(2,reg_x,_zp());  break;
    case 0xEC: _cmp(3,reg_x,_abs()); break;

    case 0xC0: _cmp(2,reg_y,_imm()); break;
    case 0xC4: _cmp(2,reg_y,_zp());  break;
    case 0xCC: _cmp(3,reg_y,_abs()); break;

    case 0x29: _and(2,_imm());  break;
    case 0x25: _and(3,_zp());   break;
    case 0x35: _and(4,_zpx());  break;
    case 0x2D: _and(4,_abs());  break;
    case 0x3D: _and(4,_abx());  break;
    case 0x39: _and(4,_aby());  break;
    case 0x21: _and(6,_zpxi()); break;
    case 0x31: _and(5,_zpiy()); break;

    case 0x09: _ora(2,_imm());  break;
    case 0x05: _ora(3,_zp());   break;
    case 0x15: _ora(4,_zpx());  break;
    case 0x0D: _ora(4,_abs());  break;
    case 0x1D: _ora(4,_abx());  break;
    case 0x19: _ora(4,_aby());  break;
    case 0x01: _ora(6,_zpxi()); break;
    case 0x11: _ora(5,_zpiy()); break;

    case 0x49: _eor(2,_imm());  break;
    case 0x45: _eor(3,_zp());   break;
    case 0x55: _eor(4,_zpx());  break;
    case 0x4D: _eor(4,_abs());  break;
    case 0x5D: _eor(4,_abx());  break;
    case 0x59: _eor(4,_aby());  break;
    case 0x41: _eor(6,_zpxi()); break;
    case 0x51: _eor(5,_zpiy()); break;

    case 0x24: _bit(3,_zp());   break;
    case 0x2C: _bit(4,_abs());  break;

      /* laod / store */
    case 0xA9: _load(2,reg_a,_imm());  break;
    case 0xA5: _load(3,reg_a,_zp());   break;
    case 0xB5: _load(4,reg_a,_zpx());  break;
    case 0xAD: _load(4,reg_a,_abs());  break;
    case 0xBD: _load(4,reg_a,_abx());  break;
    case 0xB9: _load(4,reg_a,_aby());  break;
    case 0xA1: _load(6,reg_a,_zpxi()); break;
    case 0xB1: _load(5,reg_a,_zpiy()); break;

    case 0xA2: _load(2,reg_x,_imm());  break;
    case 0xA6: _load(3,reg_x,_zp());   break;
    case 0xB6: _load(4,reg_x,_zpy());  break;
    case 0xAE: _load(4,reg_x,_abs());  break;
    case 0xBE: _load(4,reg_x,_aby());  break;

    case 0xA0: _load(2,reg_y,_imm());  break;
    case 0xA4: _load(3,reg_y,_zp());   break;
    case 0xB4: _load(4,reg_y,_zpx());  break;
    case 0xAC: _load(4,reg_y,_abs());  break;
    case 0xBC: _load(4,reg_y,_abx());  break;

    case 0x85: _store(3,reg_a,_zp());   break;
    case 0x95: _store(4,reg_a,_zpx());  break;
    case 0x8D: _store(4,reg_a,_abs());  break;
    case 0x9D: _store(5,reg_a,_abx());  break;
    case 0x99: _store(5,reg_a,_aby());  break;
    case 0x81: _store(6,reg_a,_zpxi()); break;
    case 0x91: _store(6,reg_a,_zpiy()); break;

    case 0x86: _store(3,reg_x,_zp());   break;
    case 0x96: _store(4,reg_x,_zpy());  break;
    case 0x8E: _store(4,reg_x,_abs());  break;

    case 0x84: _store(3,reg_y,_zp());   break;
    case 0x94: _store(4,reg_y,_zpx());  break;
    case 0x8C: _store(4,reg_y,_abs());  break;

      /* transfer */
    case 0xAA: _mov(2,reg_x,reg_a); break; // TAX
    case 0xA8: _mov(2,reg_y,reg_a); break; // TAY
    case 0x8A: _mov(2,reg_a,reg_x); break; // TXA
    case 0x98: _mov(2,reg_a,reg_y); break; // TYA
    case 0xBA: _mov(2,reg_x,reg_s); break; // TSX
    case 0x9A: reg_s=reg_x;rest-=2; break; // TXS

      /* shift */
    case 0x0A: _asla(2);       break;
    case 0x06: _asl(5,_zp());  break;
    case 0x16: _asl(6,_zpx()); break;
    case 0x0E: _asl(6,_abs()); break;
    case 0x1E: _asl(7,_abx()); break;

    case 0x4A: _lsra(2);       break;
    case 0x46: _lsr(5,_zp());  break;
    case 0x56: _lsr(6,_zpx()); break;
    case 0x4E: _lsr(6,_abs()); break;
    case 0x5E: _lsr(7,_abx()); break;

    case 0x2A: _rola(2);       break;
    case 0x26: _rol(5,_zp());  break;
    case 0x36: _rol(6,_zpx()); break;
    case 0x2E: _rol(6,_abs()); break;
    case 0x3E: _rol(7,_abx()); break;

    case 0x6A: _rora(2);       break;
    case 0x66: _ror(5,_zp());  break;
    case 0x76: _ror(6,_zpx()); break;
    case 0x6E: _ror(6,_abs()); break;
    case 0x7E: _ror(7,_abx()); break;

    case 0xE6: _inc(5,_zp());  break;
    case 0xF6: _inc(6,_zpx()); break;
    case 0xEE: _inc(6,_abs()); break;
    case 0xFE: _inc(7,_abx()); break;
    case 0xE8: _incr(2,reg_x); break;
    case 0xC8: _incr(2,reg_y); break;

    case 0xC6: _dec(5,_zp());  break;
    case 0xD6: _dec(6,_zpx()); break;
    case 0xCE: _dec(6,_abs()); break;
    case 0xDE: _dec(7,_abx()); break;
    case 0xCA: _decr(2,reg_x); break;
    case 0x88: _decr(2,reg_y); break;

      /* branch */
    case 0x90: _bra(2,!c_flag); break; // BCC
    case 0xB0: _bra(2, c_flag); break; // BCS
    case 0xD0: _bra(2,!z_flag); break; // BNE
    case 0xF0: _bra(2, z_flag); break; // BEQ
    case 0x10: _bra(2,!n_flag); break; // BPL
    case 0x30: _bra(2, n_flag); break; // BMI
    case 0x50: _bra(2,!v_flag); break; // BVC
    case 0x70: _bra(2, v_flag); break; // BVS

      /* jump / call / return */
    case 0x4C: reg_pc=_abs() ;rest-=3; break; // JMP abs
    case 0x6C: reg_pc=_absi();rest-=5; break; // JMP (abs)

    case 0x20: _push16(reg_pc+1);reg_pc=_abs();rest-=6; break; // JSR

    case 0x60: reg_pc=_pop16()+1;rest-=6; break; // RTS
    case 0x40: _unbind_flags(_pop8());reg_pc=_pop16();rest-=6; break; // RTI

      /* flag */
    case 0x38: c_flag=1;rest-=2; break; // SEC
    case 0xF8: d_flag=1;rest-=2; break; // SED
    case 0x78: i_flag=1;rest-=2; break; // SEI

    case 0x18: c_flag=0;rest-=2; break; // CLC
    case 0xD8: d_flag=0;rest-=2; break; // CLD
    case 0x58: i_flag=0;rest-=2; break; // CLI (この瞬間に割り込みがかかるかも知れん…)
    case 0xB8: v_flag=0;rest-=2; break; // CLV

      /* stack */
    case 0x48: _push8(reg_a);rest-=3; break; // PHA
    case 0x08: _push8(_bind_flags());rest-=3; break; // PHP
    case 0x68: reg_a=_pop8();n_flag=reg_a>>7;z_flag=reg_a==0;rest-=4; break; // PLA
    case 0x28: _unbind_flags(_pop8());rest-=4; break; // PLP

      /* others */
    case 0x00: // BRK
      b_flag=1;
      reg_pc++;
      exec_irq(IRQ);
      break;

    case 0xEA: rest-=2; break; // NOP

    default:
      cout<<"undefined opcode: "<<setfill('0')<<setw(2)<<hex<<(int)opc<<dec<<endl;
      break;
    }
  }while(rest>0);
}

void cpu::exec_irq(IRQ_TYPE it)
{
  if (logging)
    cout<<(it==RESET?"RESET":it==NMI?"NMI":"IRQ")<<" occured !"<<endl;

  u16 vect=(
    (it==RESET)?0xFFFC:
    (it==NMI  )?0xFFFA:
    (it==IRQ  )?0xFFFE:0xFFFE);
  _push16(reg_pc);
  _push8(_bind_flags());
  i_flag=1;
  reg_pc=read16(vect);
  rest-=7;
}

s64 cpu::get_master_clock()
{
  return mclock-rest;
}

double cpu::get_frequency()
{
  return 3579545.0/2;
}

void cpu::serialize(state_data &sd)
{
  sd["CPU"]<<reg_a<<reg_x<<reg_y<<reg_s<<reg_pc;
  sd["CPU"]<<c_flag<<z_flag<<i_flag<<d_flag<<b_flag<<v_flag<<n_flag;
  sd["CPU"]<<rest<<mclock<<nmi_line<<irq_line<<reset_line;
}

static string disasm(u16 pc,u8 opc,u16 opr)
{
  static const char *mne[]={
    "BRK","ORA","UNK","UNK","UNK","ORA","ASL","UNK", "PHP","ORA","ASL","UNK","UNK","ORA","ASL","UNK",
    "BPL","ORA","UNK","UNK","UNK","ORA","ASL","UNK", "CLC","ORA","UNK","UNK","UNK","ORA","ASL","UNK",
    "JSR","AND","UNK","UNK","BIT","AND","ROL","UNK", "PLP","AND","ROL","UNK","BIT","AND","ROL","UNK",
    "BMI","AND","UNK","UNK","UNK","AND","ROL","UNK", "SEC","AND","UNK","UNK","UNK","AND","ROL","UNK",
    "RTI","EOR","UNK","UNK","UNK","EOR","LSR","UNK", "PHA","EOR","LSR","UNK","JMP","EOR","LSR","UNK",
    "BVC","EOR","UNK","UNK","UNK","EOR","LSR","UNK", "CLI","EOR","UNK","UNK","UNK","EOR","LSR","UNK",
    "RTS","ADC","UNK","UNK","UNK","ADC","ROR","UNK", "PLA","ADC","ROR","UNK","JMP","ADC","ROR","UNK",
    "BVS","ADC","UNK","UNK","UNK","ADC","ROR","UNK", "SEI","ADC","UNK","UNK","UNK","ADC","ROR","UNK",
    "UNK","STA","UNK","UNK","STY","STA","STX","UNK", "DEY","UNK","TXA","UNK","STY","STA","STX","UNK",
    "BCC","STA","UNK","UNK","STY","STA","STX","UNK", "TYA","STA","TXS","UNK","UNK","STA","UNK","UNK",
    "LDY","LDA","LDX","UNK","LDY","LDA","LDX","UNK", "TAY","LDA","TAX","UNK","LDY","LDA","LDX","UNK",
    "BCS","LDA","UNK","UNK","LDY","LDA","LDX","UNK", "CLV","LDA","TSX","UNK","LDY","LDA","LDX","UNK",
    "CPY","CMP","UNK","UNK","CPY","CMP","DEC","UNK", "INY","CMP","DEX","UNK","CPY","CMP","DEC","UNK",
    "BNE","CMP","UNK","UNK","UNK","CMP","DEC","UNK", "CLD","CMP","UNK","UNK","UNK","CMP","DEC","UNK",
    "CPX","SBC","UNK","UNK","CPX","SBC","INC","UNK", "INX","SBC","NOP","UNK","CPX","SBC","INC","UNK",
    "BEQ","SBC","UNK","UNK","UNK","SBC","INC","UNK", "SED","SBC","UNK","UNK","UNK","SBC","INC","UNK",
  };
  static const int adr[]={
     0,12, 0, 0, 0, 8, 8, 0,  0, 1, 2, 0, 0, 3, 3, 0,
    14,13, 0, 0, 0, 9, 9, 0,  0, 5, 0, 0, 0, 4, 4, 0,
     3,12, 0, 0, 8, 8, 8, 0,  0, 1, 2, 0, 3, 3, 3, 0,
    14,13, 0, 0, 0, 9, 9, 0,  0, 5, 0, 0, 0, 4, 4, 0,
     0,12, 0, 0, 0, 8, 8, 0,  0, 1, 2, 0, 3, 3, 3, 0,
    14,13, 0, 0, 0, 9, 9, 0,  0, 5, 0, 0, 0, 4, 4, 0,
     0,12, 0, 0, 0, 8, 8, 0,  0, 1, 2, 0, 7, 3, 3, 0,
    14,13, 0, 0, 0, 9, 9, 0,  0, 5, 0, 0, 0, 4, 4, 0,
     0,12, 0, 0, 8, 8, 8, 0,  0, 0, 0, 0, 3, 3, 3, 0,
    14,13, 0, 0, 9, 9,10, 0,  0, 5, 0, 0, 0, 4, 0, 0,
     1,12, 1, 0, 8, 8, 8, 0,  0, 1, 0, 0, 3, 3, 3, 0,
    14,13, 0, 0, 9, 9,10, 0,  0, 5, 0, 0, 4, 4, 5, 0,
     1,12, 0, 0, 8, 8, 8, 0,  0, 1, 0, 0, 3, 3, 3, 0,
    14,13, 0, 0, 0, 9, 9, 0,  0, 5, 0, 0, 0, 4, 4, 0,
     1,12, 0, 0, 8, 8, 8, 0,  0, 1, 0, 0, 3, 3, 3, 0,
    14,13, 0, 0, 0, 9, 9, 0,  0, 5, 0, 0, 0, 4, 4, 0,
  };

  ostringstream oss;
  oss<<setiosflags(ios::uppercase)<<setfill('0')<<hex;

  switch(adr[opc]){
  case 0: // Implied
    oss<<mne[opc];
    break;
  case 1: // Immidiate #$xx
    oss<<mne[opc]<<" #$"<<setw(2)<<(opr&0xff);
    break;
  case 2: // Accumerate
    oss<<mne[opc]<<" A";
    break;
  case 3: // Absolute $xxxx
    oss<<mne[opc]<<" $"<<setw(4)<<opr;
    break;
  case 4: // Absolute X $xxxx,X
    oss<<mne[opc]<<" $"<<setw(4)<<opr<<",X";
    break;
  case 5: // Absolute Y $xxxx,Y
    oss<<mne[opc]<<" $"<<setw(4)<<opr<<",Y";
    break;
  case 6: // Absolute X indirected ($xxxx,X)
    oss<<mne[opc]<<" ($"<<setw(4)<<opr<<",X)";
    break;
  case 7: // Absolute indirected
    oss<<mne[opc]<<" ($"<<setw(4)<<opr<<")";
    break;
  case 8: // Zero page $xx
    oss<<mne[opc]<<" $"<<setw(2)<<(opr&0xff);
    break;
  case 9: // Zero page indexed X $xx,X
    oss<<mne[opc]<<" $"<<setw(2)<<(opr&0xff)<<",X";
    break;
  case 10: // Zero page indexed Y $xx,Y
    oss<<mne[opc]<<" $"<<setw(2)<<(opr&0xff)<<",Y";
    break;
  case 11: // Zero page indirected ($xx)
    oss<<mne[opc]<<" ($"<<setw(2)<<(opr&0xff)<<")";
    break;
  case 12: // Zero page indexed indirected ($xx,X)
    oss<<mne[opc]<<" ($"<<setw(2)<<(opr&0xff)<<",X)";
    break;
  case 13: // Zero page indirected indexed ($xx),Y
    oss<<mne[opc]<<" ($"<<setw(2)<<(opr&0xff)<<"),Y";
    break;
  case 14: // Relative
    oss<<mne[opc]<<" $"<<setw(4)<<(u16)(pc+(s8)(opr&0xff)+2);
    break;
  }

  return oss.str();
}

void cpu::log()
{
  u8 opc=read8(reg_pc);
  u16 opr=read16(reg_pc+1);
  string s=disasm(reg_pc,opc,opr);

  cout<<hex<<setiosflags(ios::uppercase);
  cout<<setfill('0')<<setw(4)<<reg_pc<<" ";
  cout<<setfill(' ')<<setw(13)<<left<<s<<" : ";
  cout<<setfill('0')<<right;
  cout<<"A:"<<setw(2)<<(int)reg_a<<" ";
  cout<<"X:"<<setw(2)<<(int)reg_x<<" ";
  cout<<"Y:"<<setw(2)<<(int)reg_y<<" ";
  cout<<"S:"<<setw(2)<<(int)reg_s<<" ";
  cout<<"P:"<<(n_flag?'N':'n')<<(v_flag?'V':'v')<<(b_flag?'B':'b');
  cout<<(d_flag?'D':'d')<<(i_flag?'I':'i')<<(z_flag?'Z':'z')<<(c_flag?'C':'c');
  cout<<endl;
}
