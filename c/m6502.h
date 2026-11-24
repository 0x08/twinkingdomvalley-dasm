/* A small 6502 CPU emulator, enough to run the game's own picture drawing
 * code. Documented instructions only, with decimal mode. Calls into the
 * BBC operating system ($FF00 upwards) are handed to os_call, and then
 * return as if the routine had executed RTS. */
#ifndef M6502_H
#define M6502_H

struct m6502 {
  unsigned char mem[0x10000];
  unsigned short pc;
  unsigned char a,x,y,s,p;
  //Called when execution reaches $FF00 or above, eg OSWRCH at $FFEE
  void (*os_call)(struct m6502 *cpu, unsigned short address);
  void *context; //For os_call's use
  int halt;      //os_call sets this to stop m6502_call
};

//Calls the subroutine at address, with the registers as set in cpu, and
//runs it until it returns, maxsteps instructions have executed or os_call
//sets halt. Returns the number of instructions executed, or -1 if it didn't return.
long m6502_call(struct m6502 *cpu, unsigned short address, long maxsteps);

//The same, as if called by a JSR just before caller: its return address
//is what's pushed, and the call ends when execution returns there.
long m6502_call_from(struct m6502 *cpu, unsigned short address, unsigned short caller, long maxsteps);

#endif
