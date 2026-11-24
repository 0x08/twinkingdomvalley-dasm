#include "m6502.h"

#define FC 0x01
#define FZ 0x02
#define FI 0x04
#define FD 0x08
#define FB 0x10
#define FU 0x20
#define FV 0x40
#define FN 0x80

#define RETURN_ADDRESS 0xFFFF //Execution reaching here means the call returned

static unsigned char rd(struct m6502 *c, unsigned short a){ return c->mem[a]; }
static void wr(struct m6502 *c, unsigned short a, unsigned char v){ c->mem[a]=v; }
static unsigned short rd16(struct m6502 *c, unsigned short a){
  return rd(c,a) | (rd(c,(unsigned short)(a+1))<<8);
}
//The 6502 doesn't carry into the high byte of an indirect pointer on a page boundary
static unsigned short rd16bug(struct m6502 *c, unsigned short a){
  return rd(c,a) | (rd(c,(a&0xFF00)|((a+1)&0xFF))<<8);
}
static void push(struct m6502 *c, unsigned char v){ wr(c,0x100|c->s,v); c->s--; }
static unsigned char pull(struct m6502 *c){ c->s++; return rd(c,0x100|c->s); }
static void nz(struct m6502 *c, unsigned char v){
  c->p=(c->p&~(FN|FZ)) | (v&FN) | (v==0?FZ:0);
}
static void setf(struct m6502 *c, unsigned char f, int on){
  if(on) c->p|=f; else c->p&=~f;
}

static void adc(struct m6502 *c, unsigned char v){
  int carry=c->p&FC;
  if(c->p&FD){//Binary coded decimal, NMOS behaviour for the flags
    int lo=(c->a&0x0F)+(v&0x0F)+carry;
    int hi=(c->a&0xF0)+(v&0xF0);
    setf(c,FZ,((c->a+v+carry)&0xFF)==0);
    if(lo>9){ lo+=6; hi+=0x10; }
    setf(c,FN,hi&0x80);
    setf(c,FV,(~(c->a^v)&(c->a^hi)&0x80)!=0);
    if(hi>0x90) hi+=0x60;
    setf(c,FC,hi>0xFF);
    c->a=(hi&0xF0)|(lo&0x0F);
  } else {
    int r=c->a+v+carry;
    setf(c,FV,(~(c->a^v)&(c->a^r)&0x80)!=0);
    setf(c,FC,r>0xFF);
    c->a=r&0xFF;
    nz(c,c->a);
  }
}

static void sbc(struct m6502 *c, unsigned char v){
  int borrow=(c->p&FC)?0:1;
  int r=c->a-v-borrow;
  setf(c,FV,((c->a^v)&(c->a^r)&0x80)!=0);
  if(c->p&FD){
    int lo=(c->a&0x0F)-(v&0x0F)-borrow;
    int hi=(c->a&0xF0)-(v&0xF0);
    if(lo<0){ lo-=6; hi-=0x10; }
    if(hi<0) hi-=0x60;
    setf(c,FC,r>=0);
    nz(c,r&0xFF);
    c->a=(hi&0xF0)|(lo&0x0F);
  } else {
    setf(c,FC,r>=0);
    c->a=r&0xFF;
    nz(c,c->a);
  }
}

static void cmp(struct m6502 *c, unsigned char reg, unsigned char v){
  setf(c,FC,reg>=v);
  nz(c,(unsigned char)(reg-v));
}

static void branch(struct m6502 *c, int cond){
  signed char off=(signed char)rd(c,c->pc++);
  if(cond)
    c->pc+=off;
}

//Operand addresses for each addressing mode; pc is past the opcode
static unsigned short zp(struct m6502 *c){ return rd(c,c->pc++); }
static unsigned short zpx(struct m6502 *c){ return (rd(c,c->pc++)+c->x)&0xFF; }
static unsigned short zpy(struct m6502 *c){ return (rd(c,c->pc++)+c->y)&0xFF; }
static unsigned short ab(struct m6502 *c){ unsigned short a=rd16(c,c->pc); c->pc+=2; return a; }
static unsigned short abx(struct m6502 *c){ return ab(c)+c->x; }
static unsigned short aby(struct m6502 *c){ return ab(c)+c->y; }
static unsigned short izx(struct m6502 *c){ return rd16bug(c,(rd(c,c->pc++)+c->x)&0xFF); }
static unsigned short izy(struct m6502 *c){ return rd16bug(c,rd(c,c->pc++))+c->y; }

//Read-modify-write operations
static unsigned char asl(struct m6502 *c, unsigned char v){ setf(c,FC,v&0x80); v<<=1; nz(c,v); return v; }
static unsigned char lsr(struct m6502 *c, unsigned char v){ setf(c,FC,v&1); v>>=1; nz(c,v); return v; }
static unsigned char rol(struct m6502 *c, unsigned char v){
  int in=c->p&FC; setf(c,FC,v&0x80); v=(v<<1)|in; nz(c,v); return v;
}
static unsigned char ror(struct m6502 *c, unsigned char v){
  int in=(c->p&FC)<<7; setf(c,FC,v&1); v=(v>>1)|in; nz(c,v); return v;
}
#define RMW(addr,op) do{ unsigned short ea=(addr); wr(c,ea,op(c,rd(c,ea))); }while(0)

//Executes one instruction. Returns 0, or -1 for an opcode it doesn't know.
static int step(struct m6502 *c){
  unsigned char op=rd(c,c->pc++);
  unsigned short ea;
  unsigned char v;
  switch(op){
    //Loads and stores
    case 0xA9: c->a=rd(c,c->pc++); nz(c,c->a); break;
    case 0xA5: c->a=rd(c,zp(c)); nz(c,c->a); break;
    case 0xB5: c->a=rd(c,zpx(c)); nz(c,c->a); break;
    case 0xAD: c->a=rd(c,ab(c)); nz(c,c->a); break;
    case 0xBD: c->a=rd(c,abx(c)); nz(c,c->a); break;
    case 0xB9: c->a=rd(c,aby(c)); nz(c,c->a); break;
    case 0xA1: c->a=rd(c,izx(c)); nz(c,c->a); break;
    case 0xB1: c->a=rd(c,izy(c)); nz(c,c->a); break;
    case 0xA2: c->x=rd(c,c->pc++); nz(c,c->x); break;
    case 0xA6: c->x=rd(c,zp(c)); nz(c,c->x); break;
    case 0xB6: c->x=rd(c,zpy(c)); nz(c,c->x); break;
    case 0xAE: c->x=rd(c,ab(c)); nz(c,c->x); break;
    case 0xBE: c->x=rd(c,aby(c)); nz(c,c->x); break;
    case 0xA0: c->y=rd(c,c->pc++); nz(c,c->y); break;
    case 0xA4: c->y=rd(c,zp(c)); nz(c,c->y); break;
    case 0xB4: c->y=rd(c,zpx(c)); nz(c,c->y); break;
    case 0xAC: c->y=rd(c,ab(c)); nz(c,c->y); break;
    case 0xBC: c->y=rd(c,abx(c)); nz(c,c->y); break;
    case 0x85: wr(c,zp(c),c->a); break;
    case 0x95: wr(c,zpx(c),c->a); break;
    case 0x8D: wr(c,ab(c),c->a); break;
    case 0x9D: wr(c,abx(c),c->a); break;
    case 0x99: wr(c,aby(c),c->a); break;
    case 0x81: wr(c,izx(c),c->a); break;
    case 0x91: wr(c,izy(c),c->a); break;
    case 0x86: wr(c,zp(c),c->x); break;
    case 0x96: wr(c,zpy(c),c->x); break;
    case 0x8E: wr(c,ab(c),c->x); break;
    case 0x84: wr(c,zp(c),c->y); break;
    case 0x94: wr(c,zpx(c),c->y); break;
    case 0x8C: wr(c,ab(c),c->y); break;
    //Transfers and stack
    case 0xAA: c->x=c->a; nz(c,c->x); break;
    case 0xA8: c->y=c->a; nz(c,c->y); break;
    case 0x8A: c->a=c->x; nz(c,c->a); break;
    case 0x98: c->a=c->y; nz(c,c->a); break;
    case 0xBA: c->x=c->s; nz(c,c->x); break;
    case 0x9A: c->s=c->x; break;
    case 0x48: push(c,c->a); break;
    case 0x68: c->a=pull(c); nz(c,c->a); break;
    case 0x08: push(c,c->p|FB|FU); break;
    case 0x28: c->p=pull(c)|FU; break;
    //Arithmetic and logic
    case 0x69: adc(c,rd(c,c->pc++)); break;
    case 0x65: adc(c,rd(c,zp(c))); break;
    case 0x75: adc(c,rd(c,zpx(c))); break;
    case 0x6D: adc(c,rd(c,ab(c))); break;
    case 0x7D: adc(c,rd(c,abx(c))); break;
    case 0x79: adc(c,rd(c,aby(c))); break;
    case 0x61: adc(c,rd(c,izx(c))); break;
    case 0x71: adc(c,rd(c,izy(c))); break;
    case 0xE9: sbc(c,rd(c,c->pc++)); break;
    case 0xE5: sbc(c,rd(c,zp(c))); break;
    case 0xF5: sbc(c,rd(c,zpx(c))); break;
    case 0xED: sbc(c,rd(c,ab(c))); break;
    case 0xFD: sbc(c,rd(c,abx(c))); break;
    case 0xF9: sbc(c,rd(c,aby(c))); break;
    case 0xE1: sbc(c,rd(c,izx(c))); break;
    case 0xF1: sbc(c,rd(c,izy(c))); break;
    case 0x29: c->a&=rd(c,c->pc++); nz(c,c->a); break;
    case 0x25: c->a&=rd(c,zp(c)); nz(c,c->a); break;
    case 0x35: c->a&=rd(c,zpx(c)); nz(c,c->a); break;
    case 0x2D: c->a&=rd(c,ab(c)); nz(c,c->a); break;
    case 0x3D: c->a&=rd(c,abx(c)); nz(c,c->a); break;
    case 0x39: c->a&=rd(c,aby(c)); nz(c,c->a); break;
    case 0x21: c->a&=rd(c,izx(c)); nz(c,c->a); break;
    case 0x31: c->a&=rd(c,izy(c)); nz(c,c->a); break;
    case 0x09: c->a|=rd(c,c->pc++); nz(c,c->a); break;
    case 0x05: c->a|=rd(c,zp(c)); nz(c,c->a); break;
    case 0x15: c->a|=rd(c,zpx(c)); nz(c,c->a); break;
    case 0x0D: c->a|=rd(c,ab(c)); nz(c,c->a); break;
    case 0x1D: c->a|=rd(c,abx(c)); nz(c,c->a); break;
    case 0x19: c->a|=rd(c,aby(c)); nz(c,c->a); break;
    case 0x01: c->a|=rd(c,izx(c)); nz(c,c->a); break;
    case 0x11: c->a|=rd(c,izy(c)); nz(c,c->a); break;
    case 0x49: c->a^=rd(c,c->pc++); nz(c,c->a); break;
    case 0x45: c->a^=rd(c,zp(c)); nz(c,c->a); break;
    case 0x55: c->a^=rd(c,zpx(c)); nz(c,c->a); break;
    case 0x4D: c->a^=rd(c,ab(c)); nz(c,c->a); break;
    case 0x5D: c->a^=rd(c,abx(c)); nz(c,c->a); break;
    case 0x59: c->a^=rd(c,aby(c)); nz(c,c->a); break;
    case 0x41: c->a^=rd(c,izx(c)); nz(c,c->a); break;
    case 0x51: c->a^=rd(c,izy(c)); nz(c,c->a); break;
    case 0x24: v=rd(c,zp(c)); setf(c,FZ,(c->a&v)==0); c->p=(c->p&~(FN|FV))|(v&(FN|FV)); break;
    case 0x2C: v=rd(c,ab(c)); setf(c,FZ,(c->a&v)==0); c->p=(c->p&~(FN|FV))|(v&(FN|FV)); break;
    //Comparisons
    case 0xC9: cmp(c,c->a,rd(c,c->pc++)); break;
    case 0xC5: cmp(c,c->a,rd(c,zp(c))); break;
    case 0xD5: cmp(c,c->a,rd(c,zpx(c))); break;
    case 0xCD: cmp(c,c->a,rd(c,ab(c))); break;
    case 0xDD: cmp(c,c->a,rd(c,abx(c))); break;
    case 0xD9: cmp(c,c->a,rd(c,aby(c))); break;
    case 0xC1: cmp(c,c->a,rd(c,izx(c))); break;
    case 0xD1: cmp(c,c->a,rd(c,izy(c))); break;
    case 0xE0: cmp(c,c->x,rd(c,c->pc++)); break;
    case 0xE4: cmp(c,c->x,rd(c,zp(c))); break;
    case 0xEC: cmp(c,c->x,rd(c,ab(c))); break;
    case 0xC0: cmp(c,c->y,rd(c,c->pc++)); break;
    case 0xC4: cmp(c,c->y,rd(c,zp(c))); break;
    case 0xCC: cmp(c,c->y,rd(c,ab(c))); break;
    //Increments and decrements
    case 0xE6: ea=zp(c); v=rd(c,ea)+1; wr(c,ea,v); nz(c,v); break;
    case 0xF6: ea=zpx(c); v=rd(c,ea)+1; wr(c,ea,v); nz(c,v); break;
    case 0xEE: ea=ab(c); v=rd(c,ea)+1; wr(c,ea,v); nz(c,v); break;
    case 0xFE: ea=abx(c); v=rd(c,ea)+1; wr(c,ea,v); nz(c,v); break;
    case 0xC6: ea=zp(c); v=rd(c,ea)-1; wr(c,ea,v); nz(c,v); break;
    case 0xD6: ea=zpx(c); v=rd(c,ea)-1; wr(c,ea,v); nz(c,v); break;
    case 0xCE: ea=ab(c); v=rd(c,ea)-1; wr(c,ea,v); nz(c,v); break;
    case 0xDE: ea=abx(c); v=rd(c,ea)-1; wr(c,ea,v); nz(c,v); break;
    case 0xE8: c->x++; nz(c,c->x); break;
    case 0xC8: c->y++; nz(c,c->y); break;
    case 0xCA: c->x--; nz(c,c->x); break;
    case 0x88: c->y--; nz(c,c->y); break;
    //Shifts and rotates
    case 0x0A: c->a=asl(c,c->a); break;
    case 0x06: RMW(zp(c),asl); break;
    case 0x16: RMW(zpx(c),asl); break;
    case 0x0E: RMW(ab(c),asl); break;
    case 0x1E: RMW(abx(c),asl); break;
    case 0x4A: c->a=lsr(c,c->a); break;
    case 0x46: RMW(zp(c),lsr); break;
    case 0x56: RMW(zpx(c),lsr); break;
    case 0x4E: RMW(ab(c),lsr); break;
    case 0x5E: RMW(abx(c),lsr); break;
    case 0x2A: c->a=rol(c,c->a); break;
    case 0x26: RMW(zp(c),rol); break;
    case 0x36: RMW(zpx(c),rol); break;
    case 0x2E: RMW(ab(c),rol); break;
    case 0x3E: RMW(abx(c),rol); break;
    case 0x6A: c->a=ror(c,c->a); break;
    case 0x66: RMW(zp(c),ror); break;
    case 0x76: RMW(zpx(c),ror); break;
    case 0x6E: RMW(ab(c),ror); break;
    case 0x7E: RMW(abx(c),ror); break;
    //Jumps, calls and branches
    case 0x4C: c->pc=rd16(c,c->pc); break;
    case 0x6C: c->pc=rd16bug(c,rd16(c,c->pc)); break;
    case 0x20: ea=rd16(c,c->pc); c->pc++; push(c,c->pc>>8); push(c,c->pc&0xFF); c->pc=ea; break;
    case 0x60: c->pc=pull(c); c->pc|=pull(c)<<8; c->pc++; break;
    case 0x40: c->p=pull(c)|FU; c->pc=pull(c); c->pc|=pull(c)<<8; break;
    case 0x10: branch(c,!(c->p&FN)); break;
    case 0x30: branch(c,c->p&FN); break;
    case 0x50: branch(c,!(c->p&FV)); break;
    case 0x70: branch(c,c->p&FV); break;
    case 0x90: branch(c,!(c->p&FC)); break;
    case 0xB0: branch(c,c->p&FC); break;
    case 0xD0: branch(c,!(c->p&FZ)); break;
    case 0xF0: branch(c,c->p&FZ); break;
    //Flags
    case 0x18: c->p&=~FC; break;
    case 0x38: c->p|=FC; break;
    case 0x58: c->p&=~FI; break;
    case 0x78: c->p|=FI; break;
    case 0xB8: c->p&=~FV; break;
    case 0xD8: c->p&=~FD; break;
    case 0xF8: c->p|=FD; break;
    case 0xEA: break;
    default: return -1; //Including BRK, which would enter the OS error handler
  }
  return 0;
}

long m6502_call(struct m6502 *c, unsigned short address, long maxsteps){
  long steps=0;
  push(c,(RETURN_ADDRESS-1)>>8);
  push(c,(RETURN_ADDRESS-1)&0xFF);
  c->pc=address;
  c->p|=FU;
  while(steps<maxsteps){
    if(c->pc==RETURN_ADDRESS)
      return steps;
    if(c->pc>=0xFF00){//An OS routine: let the caller handle it, then return
      if(c->os_call)
        c->os_call(c,c->pc);
      c->pc=pull(c);
      c->pc|=pull(c)<<8;
      c->pc++;
      continue;
    }
    if(step(c)<0)
      return -1;
    steps++;
  }
  return -1;
}
