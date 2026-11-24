// Twin Kingdom Valley in the browser: the original game's 6502 code running
// in an emulator, with enough of the BBC Micro's operating system for it.
// The CPU follows c/m6502.c and the graphics c/pictures.c.
'use strict';

// ---------------------------------------------------------------- 6502 CPU

const FC = 0x01, FZ = 0x02, FI = 0x04, FD = 0x08, FB = 0x10, FU = 0x20, FV = 0x40, FN = 0x80;

class CPU {
  constructor() {
    this.mem = new Uint8Array(0x10000);
    this.pc = 0; this.a = 0; this.x = 0; this.y = 0; this.s = 0xFF; this.p = FU;
  }
  rd16(a) { return this.mem[a & 0xFFFF] | (this.mem[(a + 1) & 0xFFFF] << 8); }
  rd16bug(a) { return this.mem[a] | (this.mem[(a & 0xFF00) | ((a + 1) & 0xFF)] << 8); }
  push(v) { this.mem[0x100 | this.s] = v; this.s = (this.s - 1) & 0xFF; }
  pull() { this.s = (this.s + 1) & 0xFF; return this.mem[0x100 | this.s]; }
  nz(v) { this.p = (this.p & ~(FN | FZ)) | (v & FN) | (v === 0 ? FZ : 0); return v; }
  setf(f, on) { if (on) this.p |= f; else this.p &= ~f; }
  adc(v) {
    const carry = this.p & FC, a = this.a;
    if (this.p & FD) {
      let lo = (a & 0x0F) + (v & 0x0F) + carry, hi = (a & 0xF0) + (v & 0xF0);
      this.setf(FZ, ((a + v + carry) & 0xFF) === 0);
      if (lo > 9) { lo += 6; hi += 0x10; }
      this.setf(FN, hi & 0x80);
      this.setf(FV, (~(a ^ v) & (a ^ hi) & 0x80) !== 0);
      if (hi > 0x90) hi += 0x60;
      this.setf(FC, hi > 0xFF);
      this.a = (hi & 0xF0) | (lo & 0x0F);
    } else {
      const r = a + v + carry;
      this.setf(FV, (~(a ^ v) & (a ^ r) & 0x80) !== 0);
      this.setf(FC, r > 0xFF);
      this.a = this.nz(r & 0xFF);
    }
  }
  sbc(v) {
    const borrow = (this.p & FC) ? 0 : 1, a = this.a, r = a - v - borrow;
    this.setf(FV, ((a ^ v) & (a ^ r) & 0x80) !== 0);
    if (this.p & FD) {
      let lo = (a & 0x0F) - (v & 0x0F) - borrow, hi = (a & 0xF0) - (v & 0xF0);
      if (lo < 0) { lo -= 6; hi -= 0x10; }
      if (hi < 0) hi -= 0x60;
      this.setf(FC, r >= 0);
      this.nz(r & 0xFF);
      this.a = (hi & 0xF0) | (lo & 0x0F);
    } else {
      this.setf(FC, r >= 0);
      this.a = this.nz(r & 0xFF);
    }
  }
  cmp(reg, v) { this.setf(FC, reg >= v); this.nz((reg - v) & 0xFF); }
  // Operand addresses; pc is past the opcode
  imm() { return this.pc++; }
  zp() { return this.mem[this.pc++]; }
  zpx() { return (this.mem[this.pc++] + this.x) & 0xFF; }
  zpy() { return (this.mem[this.pc++] + this.y) & 0xFF; }
  ab() { const a = this.rd16(this.pc); this.pc = (this.pc + 2) & 0xFFFF; return a; }
  abx() { return (this.ab() + this.x) & 0xFFFF; }
  aby() { return (this.ab() + this.y) & 0xFFFF; }
  izx() { return this.rd16bug((this.mem[this.pc++] + this.x) & 0xFF); }
  izy() { return (this.rd16bug(this.mem[this.pc++]) + this.y) & 0xFFFF; }
  asl(v) { this.setf(FC, v & 0x80); return this.nz((v << 1) & 0xFF); }
  lsr(v) { this.setf(FC, v & 1); return this.nz(v >> 1); }
  rol(v) { const c = this.p & FC; this.setf(FC, v & 0x80); return this.nz(((v << 1) | c) & 0xFF); }
  ror(v) { const c = (this.p & FC) << 7; this.setf(FC, v & 1); return this.nz((v >> 1) | c); }
  rmw(ea, f) { this.mem[ea] = f.call(this, this.mem[ea]); }
  branch(cond) {
    const off = this.mem[this.pc++];
    if (cond) this.pc = (this.pc + (off < 128 ? off : off - 256)) & 0xFFFF;
  }
  bit(v) { this.setf(FZ, (this.a & v) === 0); this.p = (this.p & ~(FN | FV)) | (v & (FN | FV)); }
  inc(ea) { this.mem[ea] = this.nz((this.mem[ea] + 1) & 0xFF); }
  dec(ea) { this.mem[ea] = this.nz((this.mem[ea] - 1) & 0xFF); }

  // Executes one instruction; returns false for an opcode it doesn't know
  step() {
    const m = this.mem, op = m[this.pc];
    this.pc = (this.pc + 1) & 0xFFFF;
    let ea;
    switch (op) {
      case 0xA9: this.a = this.nz(m[this.imm()]); break;
      case 0xA5: this.a = this.nz(m[this.zp()]); break;
      case 0xB5: this.a = this.nz(m[this.zpx()]); break;
      case 0xAD: this.a = this.nz(m[this.ab()]); break;
      case 0xBD: this.a = this.nz(m[this.abx()]); break;
      case 0xB9: this.a = this.nz(m[this.aby()]); break;
      case 0xA1: this.a = this.nz(m[this.izx()]); break;
      case 0xB1: this.a = this.nz(m[this.izy()]); break;
      case 0xA2: this.x = this.nz(m[this.imm()]); break;
      case 0xA6: this.x = this.nz(m[this.zp()]); break;
      case 0xB6: this.x = this.nz(m[this.zpy()]); break;
      case 0xAE: this.x = this.nz(m[this.ab()]); break;
      case 0xBE: this.x = this.nz(m[this.aby()]); break;
      case 0xA0: this.y = this.nz(m[this.imm()]); break;
      case 0xA4: this.y = this.nz(m[this.zp()]); break;
      case 0xB4: this.y = this.nz(m[this.zpx()]); break;
      case 0xAC: this.y = this.nz(m[this.ab()]); break;
      case 0xBC: this.y = this.nz(m[this.abx()]); break;
      case 0x85: m[this.zp()] = this.a; break;
      case 0x95: m[this.zpx()] = this.a; break;
      case 0x8D: m[this.ab()] = this.a; break;
      case 0x9D: m[this.abx()] = this.a; break;
      case 0x99: m[this.aby()] = this.a; break;
      case 0x81: m[this.izx()] = this.a; break;
      case 0x91: m[this.izy()] = this.a; break;
      case 0x86: m[this.zp()] = this.x; break;
      case 0x96: m[this.zpy()] = this.x; break;
      case 0x8E: m[this.ab()] = this.x; break;
      case 0x84: m[this.zp()] = this.y; break;
      case 0x94: m[this.zpx()] = this.y; break;
      case 0x8C: m[this.ab()] = this.y; break;
      case 0xAA: this.x = this.nz(this.a); break;
      case 0xA8: this.y = this.nz(this.a); break;
      case 0x8A: this.a = this.nz(this.x); break;
      case 0x98: this.a = this.nz(this.y); break;
      case 0xBA: this.x = this.nz(this.s); break;
      case 0x9A: this.s = this.x; break;
      case 0x48: this.push(this.a); break;
      case 0x68: this.a = this.nz(this.pull()); break;
      case 0x08: this.push(this.p | FB | FU); break;
      case 0x28: this.p = this.pull() | FU; break;
      case 0x69: this.adc(m[this.imm()]); break;
      case 0x65: this.adc(m[this.zp()]); break;
      case 0x75: this.adc(m[this.zpx()]); break;
      case 0x6D: this.adc(m[this.ab()]); break;
      case 0x7D: this.adc(m[this.abx()]); break;
      case 0x79: this.adc(m[this.aby()]); break;
      case 0x61: this.adc(m[this.izx()]); break;
      case 0x71: this.adc(m[this.izy()]); break;
      case 0xE9: this.sbc(m[this.imm()]); break;
      case 0xE5: this.sbc(m[this.zp()]); break;
      case 0xF5: this.sbc(m[this.zpx()]); break;
      case 0xED: this.sbc(m[this.ab()]); break;
      case 0xFD: this.sbc(m[this.abx()]); break;
      case 0xF9: this.sbc(m[this.aby()]); break;
      case 0xE1: this.sbc(m[this.izx()]); break;
      case 0xF1: this.sbc(m[this.izy()]); break;
      case 0x29: this.a = this.nz(this.a & m[this.imm()]); break;
      case 0x25: this.a = this.nz(this.a & m[this.zp()]); break;
      case 0x35: this.a = this.nz(this.a & m[this.zpx()]); break;
      case 0x2D: this.a = this.nz(this.a & m[this.ab()]); break;
      case 0x3D: this.a = this.nz(this.a & m[this.abx()]); break;
      case 0x39: this.a = this.nz(this.a & m[this.aby()]); break;
      case 0x21: this.a = this.nz(this.a & m[this.izx()]); break;
      case 0x31: this.a = this.nz(this.a & m[this.izy()]); break;
      case 0x09: this.a = this.nz(this.a | m[this.imm()]); break;
      case 0x05: this.a = this.nz(this.a | m[this.zp()]); break;
      case 0x15: this.a = this.nz(this.a | m[this.zpx()]); break;
      case 0x0D: this.a = this.nz(this.a | m[this.ab()]); break;
      case 0x1D: this.a = this.nz(this.a | m[this.abx()]); break;
      case 0x19: this.a = this.nz(this.a | m[this.aby()]); break;
      case 0x01: this.a = this.nz(this.a | m[this.izx()]); break;
      case 0x11: this.a = this.nz(this.a | m[this.izy()]); break;
      case 0x49: this.a = this.nz(this.a ^ m[this.imm()]); break;
      case 0x45: this.a = this.nz(this.a ^ m[this.zp()]); break;
      case 0x55: this.a = this.nz(this.a ^ m[this.zpx()]); break;
      case 0x4D: this.a = this.nz(this.a ^ m[this.ab()]); break;
      case 0x5D: this.a = this.nz(this.a ^ m[this.abx()]); break;
      case 0x59: this.a = this.nz(this.a ^ m[this.aby()]); break;
      case 0x41: this.a = this.nz(this.a ^ m[this.izx()]); break;
      case 0x51: this.a = this.nz(this.a ^ m[this.izy()]); break;
      case 0x24: this.bit(m[this.zp()]); break;
      case 0x2C: this.bit(m[this.ab()]); break;
      case 0xC9: this.cmp(this.a, m[this.imm()]); break;
      case 0xC5: this.cmp(this.a, m[this.zp()]); break;
      case 0xD5: this.cmp(this.a, m[this.zpx()]); break;
      case 0xCD: this.cmp(this.a, m[this.ab()]); break;
      case 0xDD: this.cmp(this.a, m[this.abx()]); break;
      case 0xD9: this.cmp(this.a, m[this.aby()]); break;
      case 0xC1: this.cmp(this.a, m[this.izx()]); break;
      case 0xD1: this.cmp(this.a, m[this.izy()]); break;
      case 0xE0: this.cmp(this.x, m[this.imm()]); break;
      case 0xE4: this.cmp(this.x, m[this.zp()]); break;
      case 0xEC: this.cmp(this.x, m[this.ab()]); break;
      case 0xC0: this.cmp(this.y, m[this.imm()]); break;
      case 0xC4: this.cmp(this.y, m[this.zp()]); break;
      case 0xCC: this.cmp(this.y, m[this.ab()]); break;
      case 0xE6: this.inc(this.zp()); break;
      case 0xF6: this.inc(this.zpx()); break;
      case 0xEE: this.inc(this.ab()); break;
      case 0xFE: this.inc(this.abx()); break;
      case 0xC6: this.dec(this.zp()); break;
      case 0xD6: this.dec(this.zpx()); break;
      case 0xCE: this.dec(this.ab()); break;
      case 0xDE: this.dec(this.abx()); break;
      case 0xE8: this.x = this.nz((this.x + 1) & 0xFF); break;
      case 0xC8: this.y = this.nz((this.y + 1) & 0xFF); break;
      case 0xCA: this.x = this.nz((this.x - 1) & 0xFF); break;
      case 0x88: this.y = this.nz((this.y - 1) & 0xFF); break;
      case 0x0A: this.a = this.asl(this.a); break;
      case 0x06: this.rmw(this.zp(), this.asl); break;
      case 0x16: this.rmw(this.zpx(), this.asl); break;
      case 0x0E: this.rmw(this.ab(), this.asl); break;
      case 0x1E: this.rmw(this.abx(), this.asl); break;
      case 0x4A: this.a = this.lsr(this.a); break;
      case 0x46: this.rmw(this.zp(), this.lsr); break;
      case 0x56: this.rmw(this.zpx(), this.lsr); break;
      case 0x4E: this.rmw(this.ab(), this.lsr); break;
      case 0x5E: this.rmw(this.abx(), this.lsr); break;
      case 0x2A: this.a = this.rol(this.a); break;
      case 0x26: this.rmw(this.zp(), this.rol); break;
      case 0x36: this.rmw(this.zpx(), this.rol); break;
      case 0x2E: this.rmw(this.ab(), this.rol); break;
      case 0x3E: this.rmw(this.abx(), this.rol); break;
      case 0x6A: this.a = this.ror(this.a); break;
      case 0x66: this.rmw(this.zp(), this.ror); break;
      case 0x76: this.rmw(this.zpx(), this.ror); break;
      case 0x6E: this.rmw(this.ab(), this.ror); break;
      case 0x7E: this.rmw(this.abx(), this.ror); break;
      case 0x4C: this.pc = this.rd16(this.pc); break;
      case 0x6C: this.pc = this.rd16bug(this.rd16(this.pc)); break;
      case 0x20:
        ea = this.rd16(this.pc); this.pc = (this.pc + 1) & 0xFFFF;
        this.push(this.pc >> 8); this.push(this.pc & 0xFF); this.pc = ea; break;
      case 0x60: this.pc = this.pull(); this.pc = ((this.pull() << 8) | this.pc) + 1 & 0xFFFF; break;
      case 0x40: this.p = this.pull() | FU; this.pc = this.pull(); this.pc |= this.pull() << 8; break;
      case 0x10: this.branch(!(this.p & FN)); break;
      case 0x30: this.branch(this.p & FN); break;
      case 0x50: this.branch(!(this.p & FV)); break;
      case 0x70: this.branch(this.p & FV); break;
      case 0x90: this.branch(!(this.p & FC)); break;
      case 0xB0: this.branch(this.p & FC); break;
      case 0xD0: this.branch(!(this.p & FZ)); break;
      case 0xF0: this.branch(this.p & FZ); break;
      case 0x18: this.p &= ~FC; break;
      case 0x38: this.p |= FC; break;
      case 0x58: this.p &= ~FI; break;
      case 0x78: this.p |= FI; break;
      case 0xB8: this.p &= ~FV; break;
      case 0xD8: this.p &= ~FD; break;
      case 0xF8: this.p |= FD; break;
      case 0xEA: break;
      default: return false;
    }
    return true;
  }
  // Returns from an OS routine as RTS would
  rts() { this.pc = this.pull(); this.pc = ((this.pull() << 8) | this.pc) + 1 & 0xFFFF; }
}

// ------------------------------------------------------------- The screen

const SCREEN5 = 0x5800, SCREEN7 = 0x7C00;
// Bytes following each VDU code, see the BBC Micro User Guide
const VDU_PARAMS = [0,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,1,2,5,0,0,1,9, 8,5,0,0,4,4,0,2];
const PHYSICAL = [[0,0,0],[255,0,0],[0,255,0],[255,255,0],[0,0,255],[255,0,255],[0,255,255],[255,255,255]];
const CSS_COLOUR = ['#000','#f00','#0f0','#ff0','#00f','#f0f','#0ff','#fff'];
// MODE 7 shows these codes differently from ASCII
const TELETEXT = {0x5B:'←', 0x5C:'½', 0x5D:'→', 0x5E:'↑', 0x5F:'―', 0x60:'£',
                  0x7B:'¼', 0x7C:'‖', 0x7D:'¾', 0x7E:'÷', 0x7F:'■'};

// CRC-32, to identify pictures when testing
const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();
function crc32(bytes) {
  let c = 0xFFFFFFFF;
  for (const b of bytes) c = CRC_TABLE[(c ^ b) & 0xFF] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}

class Machine {
  // opts: image (Uint8Array, from $0400), screen and canvas elements,
  // script (test mode: lines of input, deterministic clock, transcript)
  constructor(opts) {
    this.cpu = new CPU();
    this.cpu.mem.set(opts.image, 0x0400);
    this.textEl = opts.textEl;
    this.canvas = opts.canvas;
    this.onMode = opts.onMode || (() => {});
    this.onWaiting = opts.onWaiting || (() => {});
    this.script = opts.script || null;
    this.transcript = [];
    this.keys = [];           // Keyboard buffer
    this.escape = false;
    this.halted = false;
    this.started = performance.now();
    this.ticks = 0;           // Clock reads, for the test mode's clock
    // VDU state
    this.mode = 7;
    this.queue = []; this.needed = 0;
    this.win = {left: 0, bottom: 24, right: 39, top: 0};
    this.col = 0; this.row = 0;
    this.gcolAction = 0; this.gcolColour = 3; this.gx = 0; this.gy = 0;
    this.palette = [0, 1, 3, 7];
    this.t = {needed: 0, command: 0, hidden: false, graphics: false, picture: false};
    this.lineInput = null;    // OSWORD 0 in progress
    this.dirty = true;
    this.clearMode7();
    this.cpu.s = 0xFD;        // As runtkv: the game is called with the stack at $FF
    this.cpu.mem[0x1FF] = 0xFF; this.cpu.mem[0x1FE] = 0xFE;
    this.cpu.pc = 0x1400;     // Where LVAL starts the game
  }

  // ---- Transcript, exactly as runtkv -w prints it, for comparing in tests.
  // This follows runtkv.c's oswrch() and change_mode().
  tr(s) { if (this.script) this.transcript.push(s); }
  transcriptByte(c) {
    const t = this.t;
    if (t.needed > 0) {
      if (--t.needed === 0) {
        if (t.command === 28) t.hidden = false;
        else if (t.command === 22) {
          if (c === 5) t.graphics = true;
          else if (t.graphics) { t.graphics = false; t.picture = true; }
        }
      }
      return;
    }
    if (t.graphics) {
      if (c < 32) { t.command = c; t.needed = VDU_PARAMS[c]; }
      return;
    }
    if (c < 32 || c === 127) {
      t.command = c;
      t.needed = c < 32 ? VDU_PARAMS[c] : 0;
      if (c === 22) t.hidden = true;
      if (t.hidden) return;
      if (c === 10) this.tr('\n');
      return;
    }
    if (t.hidden) return;
    if (c >= 128 && c < 160) this.tr(' ');
    else { const k = c >= 160 ? c - 128 : c; this.tr(k === 0x5F ? '-' : String.fromCharCode(k)); }
  }
  // The picture as runtkv identifies it with TKV_PICTURE_HASH
  pictureToken() {
    const p = this.pixels(), all = new Uint8Array(p.length + 4);
    all.set(p); all.set(this.palette, p.length);
    this.tr('\n[picture:' + crc32(all).toString(16).padStart(8, '0') + ']\n');
  }

  // ---- MODE 7: text in screen memory at $7C00, 40 by 25
  clearMode7() {
    this.cpu.mem.fill(0x20, SCREEN7, SCREEN7 + 1000);
    this.win = {left: 0, bottom: 24, right: 39, top: 0};
    this.col = 0; this.row = 0;
    this.dirty = true;
  }
  scrollUp() {
    const m = this.cpu.mem, w = this.win;
    for (let r = w.top; r < w.bottom; r++)
      m.copyWithin(SCREEN7 + r * 40 + w.left, SCREEN7 + (r + 1) * 40 + w.left, SCREEN7 + (r + 1) * 40 + w.right + 1);
    m.fill(0x20, SCREEN7 + w.bottom * 40 + w.left, SCREEN7 + w.bottom * 40 + w.right + 1);
  }
  lineFeed() {
    if (this.row < this.win.bottom) this.row++; else this.scrollUp();
  }
  putChar(c) {
    if (this.mode !== 7) return;
    this.cpu.mem[SCREEN7 + this.row * 40 + this.col] = c;
    this.dirty = true;
    if (++this.col > this.win.right) { this.col = this.win.left; this.lineFeed(); }
  }

  // ---- MODE 5: pictures, as in c/pictures.c
  plot(px, py) {
    if (px < 0 || px >= 160 || py < 0 || py >= 256) return;
    const a = SCREEN5 + (py >> 3) * 320 + (px >> 2) * 8 + (py & 7), s = px & 3;
    const mask = 0x88 >> s, c = this.gcolColour;
    const bits = ((c & 2) ? 0x80 >> s : 0) | ((c & 1) ? 0x08 >> s : 0);
    const m = this.cpu.mem;
    switch (this.gcolAction) {
      case 0: m[a] = (m[a] & ~mask) | bits; break;
      case 1: m[a] |= bits; break;
      case 2: m[a] &= bits | (~mask & 0xFF); break;
      case 3: m[a] ^= bits; break;
      case 4: m[a] ^= mask; break;
    }
  }
  line(x0, y0, x1, y1) {
    x0 >>= 3; x1 >>= 3; y0 = 255 - (y0 >> 2); y1 = 255 - (y1 >> 2);
    const dx = Math.abs(x1 - x0), dy = -Math.abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    let e = dx + dy;
    for (;;) {
      this.plot(x0, y0);
      if (x0 === x1 && y0 === y1) break;
      const e2 = 2 * e;
      if (e2 >= dy) { e += dy; x0 += sx; }
      if (e2 <= dx) { e += dx; y0 += sy; }
    }
  }
  pixels() {  // Logical colours, top row first
    const m = this.cpu.mem, out = new Uint8Array(160 * 256);
    for (let y = 0; y < 256; y++)
      for (let x = 0; x < 160; x++) {
        const b = m[SCREEN5 + (y >> 3) * 320 + (x >> 2) * 8 + (y & 7)], s = x & 3;
        out[y * 160 + x] = ((b & (0x80 >> s)) ? 2 : 0) | ((b & (0x08 >> s)) ? 1 : 0);
      }
    return out;
  }

  // ---- VDU commands
  vdu(q) {
    const c = q[0];
    switch (c) {
      case 22: this.setMode(q[1]); break;
      case 28: this.win = {left: q[1], bottom: q[2], right: q[3], top: q[4]};
               this.col = this.win.left; this.row = this.win.top; break;
      case 31: this.col = this.win.left + q[1]; this.row = this.win.top + q[2]; break;
      case 30: this.col = this.win.left; this.row = this.win.top; break;
      case 12:
        if (this.mode === 7) for (let r = this.win.top; r <= this.win.bottom; r++)
          this.cpu.mem.fill(0x20, SCREEN7 + r * 40 + this.win.left, SCREEN7 + r * 40 + this.win.right + 1);
        this.col = this.win.left; this.row = this.win.top; this.dirty = true; break;
      case 18: this.gcolAction = q[1]; this.gcolColour = q[2] & 3; break;
      case 19: this.palette[q[1] & 3] = q[2] & 7; this.dirty = true; break;
      case 25: {
        const k = q[1];
        let x = (q[2] | (q[3] << 8)) << 16 >> 16, y = (q[4] | (q[5] << 8)) << 16 >> 16;
        if ((k & 4) === 0) { x += this.gx; y += this.gy; }
        if (k < 8 && (k & 3) !== 0 && this.mode === 5) { this.line(this.gx, this.gy, x, y); this.dirty = true; }
        this.gx = x; this.gy = y;
        break;
      }
      case 10: if (this.mode === 7) this.lineFeed(); break;
      case 13: this.col = this.win.left; break;
      case 8: if (this.col > this.win.left) this.col--; break;
      case 127:
        if (this.col > this.win.left) { this.col--; this.cpu.mem[SCREEN7 + this.row * 40 + this.col] = 0x20; this.dirty = true; }
        break;
    }
  }
  setMode(n) {
    if (this.t.picture) { this.t.picture = false; this.pictureToken(); }
    this.mode = n === 5 ? 5 : 7;
    this.palette = [0, 1, 3, 7];
    if (this.mode === 5) this.cpu.mem.fill(0, SCREEN5, 0x8000);
    else this.clearMode7();
    this.onMode(this.mode);
    this.dirty = true;
  }
  oswrch(c) {
    if (this.script) this.transcriptByte(c);
    if (this.needed > 0) {
      this.queue.push(c);
      if (--this.needed === 0) this.vdu(this.queue);
      return;
    }
    if (c < 32 || c === 127) {
      this.queue = [c];
      this.needed = c < 32 ? VDU_PARAMS[c] : 0;
      if (this.needed === 0) this.vdu(this.queue);
      return;
    }
    this.putChar(c);
  }
  echo(c) { this.oswrchNoTranscript(c); }
  oswrchNoTranscript(c) {
    const s = this.script; this.script = null;
    this.oswrch(c);
    this.script = s;
  }

  // ---- Input
  key(code) {   // From the page: ASCII, 13 Return, 127 Delete, 9 TAB, 27 Escape
    if (code === 27) { this.escape = true; this.cpu.mem[0xFF] |= 0x80; }
    else this.keys.push(code);
  }
  keysWaiting() {
    if (this.script) return 1;  // As runtkv: the next read ends the game if there's no more
    return this.keys.length;
  }
  // Test mode reads a line at a time, as runtkv does
  scriptLine() { return this.script.length ? this.script.shift() : null; }

  // OSWORD 0: returns false to wait for more keys
  readLine(block) {
    const m = this.cpu.mem, buf = m[block] | (m[block + 1] << 8), max = m[block + 2];
    if (this.script) {
      const line = this.scriptLine();
      if (line === null) { this.halt(); return true; }
      const s = line.slice(0, max);
      for (let i = 0; i < s.length; i++) m[buf + i] = s.charCodeAt(i) & 0x7F;
      m[buf + s.length] = 13;
      this.cpu.y = s.length; this.cpu.p &= ~FC;
      return true;
    }
    if (!this.lineInput) this.lineInput = [];
    while (this.keys.length) {
      const k = this.keys.shift();
      if (this.escape) break;
      if (k === 13) {
        const s = this.lineInput;
        s.forEach((ch, i) => { m[buf + i] = ch; });
        m[buf + s.length] = 13;
        this.cpu.y = s.length; this.cpu.p &= ~FC;
        this.lineInput = null;
        this.echo(13); this.echo(10);
        return true;
      }
      if (k === 127 || k === 8) {
        if (this.lineInput.length) { this.lineInput.pop(); this.echo(127); }
      } else if (k >= 32 && k < 127 && this.lineInput.length < max) {
        this.lineInput.push(k); this.echo(k);
      }
    }
    if (this.escape) {  // Escape abandons the line, carry set
      this.escape = false; this.cpu.mem[0xFF] &= 0x7F;
      this.lineInput = null; this.cpu.p |= FC; this.echo(13); this.echo(10);
      return true;
    }
    return false;
  }
  // OSRDCH: returns false to wait for a key
  readKey() {
    if (this.script) {
      const line = this.scriptLine();
      if (line === null) { this.halt(); return true; }
      this.cpu.a = line.length ? line.charCodeAt(0) & 0x7F : 13;
      this.cpu.p &= ~FC;
      return true;
    }
    if (!this.keys.length) return false;
    this.cpu.a = this.keys.shift(); this.cpu.p &= ~FC;
    return true;
  }
  // The game waits by polling the clock until it reaches an exact value
  // ($2282, $3B20), which a real BBC polls fast enough never to miss. So the
  // clock advances by at most one centisecond each time it's read.
  clock() {
    if (this.script) return ++this.ticks;
    const now = Math.floor((performance.now() - this.started) / 10);
    this.ticks = Math.min(now, this.ticks + 1);
    return this.ticks;
  }

  // ---- *SAVE and *LOAD: OSFILE, kept in the browser's storage
  osfile(block) {
    const m = this.cpu.mem, a = this.cpu.a;
    const word = o => m[block + o] | (m[block + o + 1] << 8);
    let name = '', p = word(0);
    while (m[p] !== 13 && name.length < 12) name += String.fromCharCode(m[p++]);
    const key = 'tkv-save-' + name;
    try {
      if (a === 0) {
        const start = word(10), end = word(14);
        localStorage.setItem(key, JSON.stringify({load: word(2), data: Array.from(m.slice(start, end))}));
      } else if (a === 0xFF) {
        const f = JSON.parse(localStorage.getItem(key) || 'null');
        if (f) m.set(f.data, m[block + 6] === 0 ? word(2) : f.load);
      }
    } catch (e) { /* No storage: nothing is saved */ }
  }

  // An OS call; returns false if it has to wait
  os(address) {
    const c = this.cpu;
    switch (address) {
      case 0xFFEE: this.oswrch(c.a); break;
      case 0xFFE3: if (c.a === 13) { this.oswrch(10); this.oswrch(13); } else this.oswrch(c.a); break;
      case 0xFFE7: this.oswrch(10); this.oswrch(13); break;
      case 0xFFE0: return this.readKey();
      case 0xFFF1: {
        const block = c.x | (c.y << 8);
        if (c.a === 0) return this.readLine(block);
        if (c.a === 1) {
          const t = this.clock();
          for (let i = 0; i < 5; i++) c.mem[block + i] = i < 4 ? Math.floor(t / 2 ** (8 * i)) & 0xFF : 0;
        }
        break;
      }
      case 0xFFF4:
        if (c.a === 0x80) {         // Characters waiting
          const n = this.keysWaiting();
          c.x = c.y = Math.min(n, 255);
          if (n === 0) this.idle = true;
        } else if (c.a === 0x81) {  // INKEY: only TAB (-97) is asked about
          const tab = !this.script && c.x === 0x9F && this.keys[0] === 9;
          c.x = c.y = tab ? 0xFF : 0;
        } else if (c.a === 0x7E || c.a === 0x7C) {
          this.escape = false; c.mem[0xFF] &= 0x7F;
        }
        break;
      case 0xFFDD: this.osfile(c.x | (c.y << 8)); break;
    }
    return true;
  }

  halt() { this.halted = true; }

  // Runs for about ms milliseconds. Returns 'input' when waiting for a key,
  // 'idle' when polling for one, 'halted', or 'running'.
  run(ms) {
    const c = this.cpu, end = performance.now() + ms;
    this.idle = false;
    if (this.showCursor) { this.showCursor = false; this.dirty = true; }
    while (!this.halted) {
      for (let i = 0; i < 4000; i++) {
        if (c.pc >= 0xFF00) {
          if (!this.os(c.pc)) { this.showCursor = true; this.dirty = true; return 'input'; }
          if (this.halted) return 'halted';
          c.rts();
          if (this.idle) { this.showCursor = this.mode === 7; this.dirty = true; return 'idle'; }
          continue;
        }
        if (!c.step()) { this.halt(); return 'halted'; }
      }
      if (performance.now() >= end) return 'running';
    }
    return 'halted';
  }

  // ---- Drawing the screen
  render() {
    // In MODE 5 the game fills shapes by writing screen memory itself, so
    // the picture is redrawn every time
    if (this.mode === 5) { this.renderPicture(); return; }
    if (!this.dirty) return;
    this.dirty = false;
    this.renderText();
  }
  renderPicture() {
    const ctx = this.canvas.getContext('2d'), img = ctx.createImageData(160, 256), p = this.pixels();
    for (let i = 0; i < p.length; i++) {
      const rgb = PHYSICAL[this.palette[p[i]]];
      img.data[i * 4] = rgb[0]; img.data[i * 4 + 1] = rgb[1]; img.data[i * 4 + 2] = rgb[2]; img.data[i * 4 + 3] = 255;
    }
    ctx.putImageData(img, 0, 0);
  }
  // Teletext: control codes show as spaces and change the colour of what
  // follows on the row; flashing and concealed text are supported
  renderText() {
    const m = this.cpu.mem, rows = [];
    for (let r = 0; r < 25; r++) {
      let fg = 7, bg = 0, flash = false, conceal = false, html = '', run = '', runStyle = '';
      const flush = () => { if (run) html += '<span' + runStyle + '>' + run + '</span>'; run = ''; };
      for (let col = 0; col < 40; col++) {
        let c = m[SCREEN7 + r * 40 + col] & 0x7F;
        let ch = ' ';
        if (c >= 32) ch = TELETEXT[c] || String.fromCharCode(c);
        if (ch === '<') ch = '&lt;'; else if (ch === '&') ch = '&amp;'; else if (ch === '>') ch = '&gt;';
        if (conceal) ch = ' ';
        const cursor = this.showCursor && r === this.row && col === this.col;
        const style = ' style="color:' + CSS_COLOUR[fg] + ';background:' + CSS_COLOUR[bg] + '"' +
                      (cursor ? ' class="cursor"' : flash ? ' class="flash"' : '');
        if (style !== runStyle) { flush(); runStyle = style; }
        run += ch;
        // "Set-after" attributes take effect from the next character
        const raw = m[SCREEN7 + r * 40 + col];
        if (raw >= 0x81 && raw <= 0x87) { fg = raw - 0x80; conceal = false; }
        else if (raw === 0x88) flash = true;
        else if (raw === 0x89) flash = false;
        else if (raw === 0x98) conceal = true;
        else if (raw === 0x9C) bg = 0;
        else if (raw === 0x9D) bg = fg;
        else if (c < 32 && raw === c) { /* Other control codes: just a space */ }
      }
      flush();
      rows.push(html);
    }
    this.textEl.innerHTML = rows.map((h, r) => '<div class="row">' + h + '</div>').join('');
  }
}

if (typeof module !== 'undefined') module.exports = {CPU, Machine, crc32};
