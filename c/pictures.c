#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "m6502.h"
#include "pictures.h"

#define IMAGE_START 0x0400
#define IMAGE_SIZE 23040
#define SCREEN 0x5800 //MODE 5 screen memory, 320 bytes per character row

//The BBC's eight physical colours
static const unsigned char physical[8][3]={
  {0,0,0},{255,0,0},{0,255,0},{255,255,0},{0,0,255},{255,0,255},{0,255,255},{255,255,255}};

//Bytes following each VDU code, see the BBC Micro User Guide
static const unsigned char vdu_params[32]={
  0,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,1,2,5,0,0,1,9, 8,5,0,0,4,4,0,2};

struct vdu {
  struct m6502 *cpu;
  unsigned char queue[10];
  int queued, needed;
  int gcol_action, gcol_colour;
  int x, y; //Graphics cursor, BBC graphics units with the origin bottom left
  unsigned char palette[4];
};

static void plot_pixel(struct vdu *v, int px, int py){
  if(px<0 || px>=PIC_W || py<0 || py>=PIC_H)
    return;
  unsigned char *b=v->cpu->mem+SCREEN+(py>>3)*320+(px>>2)*8+(py&7);
  int shift=px&3;
  unsigned char mask=0x88>>shift; //A MODE 5 pixel uses bits 7-shift and 3-shift
  unsigned char bits=((v->gcol_colour&2)?0x80>>shift:0) | ((v->gcol_colour&1)?0x08>>shift:0);
  switch(v->gcol_action){
    case 0: *b=(*b&~mask)|bits; break;
    case 1: *b|=bits; break;
    case 2: *b&=bits|~mask; break;
    case 3: *b^=bits; break;
    case 4: *b^=mask; break;
  }
}

//A line in graphics units, plotting both ends
static void draw_line(struct vdu *v, int x0, int y0, int x1, int y1){
  x0>>=3; x1>>=3; //8 graphics units per pixel across, 4 down
  y0=PIC_H-1-(y0>>2); y1=PIC_H-1-(y1>>2);
  int dx=abs(x1-x0), dy=-abs(y1-y0);
  int sx=x0<x1?1:-1, sy=y0<y1?1:-1;
  int e=dx+dy;
  for(;;){
    plot_pixel(v,x0,y0);
    if(x0==x1 && y0==y1)
      break;
    int e2=2*e;
    if(e2>=dy){ e+=dy; x0+=sx; }
    if(e2<=dx){ e+=dx; y0+=sy; }
  }
}

static void vdu_command(struct vdu *v){
  unsigned char *q=v->queue;
  switch(q[0]){
    case 22://MODE: clear the screen and reset the palette
      memset(v->cpu->mem+SCREEN,0,0x8000-SCREEN);
      v->palette[0]=0; v->palette[1]=1; v->palette[2]=3; v->palette[3]=7;
      break;
    case 18://GCOL action, colour
      v->gcol_action=q[1];
      v->gcol_colour=q[2]&3;
      break;
    case 19://Palette: logical, physical
      v->palette[q[1]&3]=q[2]&7;
      break;
    case 25: {//PLOT k, x, y
      int k=q[1];
      int x=(short)(q[2]|(q[3]<<8)), y=(short)(q[4]|(q[5]<<8));
      if( (k&4)==0 ){//Relative
        x+=v->x;
        y+=v->y;
      }
      if(k<8 && (k&3)!=0)//Draw a line in the GCOL colour
        draw_line(v,v->x,v->y,x,y);
      v->x=x;
      v->y=y;
      break;
    }
  }
}

void vdu_write(struct vdu *v, unsigned char c){
  if(v->needed>0){
    v->queue[v->queued++]=c;
    if(--v->needed==0)
      vdu_command(v);
    return;
  }
  v->queue[0]=c;
  v->queued=1;
  v->needed = c<32 ? vdu_params[c] : 0;
  if(v->needed==0)
    vdu_command(v);
}

struct vdu * vdu_create(struct m6502 *cpu){
  struct vdu *v=calloc(1,sizeof(struct vdu));
  if(v==NULL)
    return NULL;
  v->cpu=cpu;
  v->palette[0]=0; v->palette[1]=1; v->palette[2]=3; v->palette[3]=7;
  return v;
}

void vdu_read_screen(const struct vdu *v, struct picture *p){
  int x,y;
  for(y=0;y<PIC_H;y++)
    for(x=0;x<PIC_W;x++){
      unsigned char b=v->cpu->mem[SCREEN+(y>>3)*320+(x>>2)*8+(y&7)];
      int s=x&3;
      p->pixel[y][x]=((b&(0x80>>s))?2:0) | ((b&(0x08>>s))?1:0);
    }
  memcpy(p->palette,v->palette,4);
}

static void os_call(struct m6502 *cpu, unsigned short address){
  if(address==0xFFEE)//OSWRCH; the drawing code makes no other OS calls
    vdu_write((struct vdu *)cpu->context,cpu->a);
}

int render_picture_in(struct m6502 *cpu, unsigned char *image, unsigned char n, struct picture *p,
                      const unsigned short *chain, int carry, long maxsteps){
  struct vdu *v=vdu_create(cpu);
  int i;
  if(v==NULL)
    return -1;
  memcpy(cpu->mem+IMAGE_START,image,IMAGE_SIZE);
  cpu->os_call=os_call;
  cpu->context=v;
  //The stack as the game would have it: its return addresses, outermost
  //first, below where the game itself was entered with the stack at $FF
  cpu->s=0xFF;
  for(i=0;chain[i+1];i++);
  cpu->mem[0x1FF]=0xFF; cpu->mem[0x1FE]=0xFE;//Whatever started the game, see runtkv
  cpu->s=0xFD;
  for(;i>0;i--){
    cpu->mem[0x100+cpu->s]=chain[i]>>8; cpu->s--;
    cpu->mem[0x100+cpu->s]=chain[i]&0xFF; cpu->s--;
  }
  cpu->x=n;
  cpu->p = carry ? cpu->p|1 : cpu->p&~1;
  long steps=m6502_call_from(cpu,0x0B12,chain[0]+1,maxsteps);//JSR $0B12 with X=picture
  vdu_read_screen(v,p);
  memcpy(image,cpu->mem+IMAGE_START,IMAGE_SIZE);
  free(v);
  return steps<0 ? -1 : 0;
}

int render_picture(const unsigned char *image, unsigned char n, struct picture *p, long maxsteps){
  struct m6502 *cpu=calloc(1,sizeof(struct m6502));
  unsigned char *copy=malloc(IMAGE_SIZE);
  int r=-1;
  if(cpu!=NULL && copy!=NULL){
    memcpy(copy,image,IMAGE_SIZE);
    r=render_picture_in(cpu,copy,n,p,picture_call_start,0,maxsteps);
  }
  free(copy);
  free(cpu);
  return r;
}

//Return addresses on the stack when the game calls $0B12, innermost first
const unsigned short picture_call_start[]={0x2526,0x144A,0};

int picture_columns(){
  const char *e=getenv("TKV_PICTURE_WIDTH");
  struct winsize w;
  int cols=80;
  if(e!=NULL && atoi(e)>0)
    return atoi(e)>PIC_W ? PIC_W : atoi(e);
  if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&w)==0 && w.ws_col>0){
    cols=w.ws_col;
    if(w.ws_row>8 && cols*PIC_H/(4*PIC_W)>w.ws_row-4)//Rows printed are cols*256/640
      cols=(w.ws_row-4)*4*PIC_W/PIC_H;
  }
  return cols>PIC_W ? PIC_W : cols;
}

void picture_rgb(const struct picture *p, int x, int y, unsigned char rgb[3]){
  memcpy(rgb,physical[p->palette[p->pixel[y][x]]],3);
}

/* PNG without compression: zlib "stored" blocks need only CRC-32 and Adler-32 */

static unsigned long crc_table[256];

static unsigned long crc(unsigned long c, const unsigned char *buf, size_t len){
  size_t i;
  if(crc_table[1]==0){
    unsigned long n,k;
    for(n=0;n<256;n++){
      unsigned long t=n;
      for(k=0;k<8;k++)
        t = (t&1) ? 0xEDB88320UL^(t>>1) : t>>1;
      crc_table[n]=t;
    }
  }
  c^=0xFFFFFFFFUL;
  for(i=0;i<len;i++)
    c=crc_table[(c^buf[i])&0xFF]^(c>>8);
  return c^0xFFFFFFFFUL;
}

static void put32(unsigned char *b, unsigned long v){
  b[0]=v>>24; b[1]=v>>16; b[2]=v>>8; b[3]=v;
}

static void chunk(FILE *fp, const char *type, const unsigned char *data, size_t len){
  unsigned char b[4];
  put32(b,len);
  fwrite(b,1,4,fp);
  unsigned long c=crc(0,(const unsigned char *)type,4);
  c=crc(c,data,len);
  fwrite(type,1,4,fp);
  fwrite(data,1,len,fp);
  put32(b,c);
  fwrite(b,1,4,fp);
}

int write_png(const char *path, const struct picture *p, int scale){
  int w=PIC_W*scale*2, h=PIC_H*scale;
  size_t rowlen=1+3*(size_t)w;
  size_t rawlen=rowlen*h;
  size_t nblocks=(rawlen+65534)/65535;
  size_t zlen=2+rawlen+5*nblocks+4;
  unsigned char *raw=malloc(rawlen), *z=malloc(zlen);
  unsigned char ihdr[13];
  int x,y,i;
  FILE *fp;
  if(raw==NULL || z==NULL){
    free(raw); free(z);
    return -1;
  }
  for(y=0;y<h;y++){
    unsigned char *r=raw+y*rowlen;
    *r++=0;//No filter
    for(x=0;x<w;x++){
      picture_rgb(p,x/(scale*2),y/scale,r);
      r+=3;
    }
  }
  //zlib stream of stored deflate blocks
  unsigned char *o=z;
  unsigned long a=1,b=0;
  *o++=0x78; *o++=0x01;
  for(i=0;i<(int)nblocks;i++){
    size_t off=(size_t)i*65535, n=rawlen-off<65535 ? rawlen-off : 65535;
    *o++= i==(int)nblocks-1;
    *o++=n&0xFF; *o++=n>>8; *o++=~n&0xFF; *o++=(~n>>8)&0xFF;
    memcpy(o,raw+off,n);
    o+=n;
  }
  for(i=0;i<(int)rawlen;i++){
    a=(a+raw[i])%65521;
    b=(b+a)%65521;
  }
  put32(o,(b<<16)|a);
  o+=4;
  put32(ihdr,w); put32(ihdr+4,h);
  ihdr[8]=8; ihdr[9]=2; ihdr[10]=0; ihdr[11]=0; ihdr[12]=0;//8-bit RGB
  fp=fopen(path,"wb");
  if(fp!=NULL){
    fwrite("\x89PNG\r\n\x1a\n",1,8,fp);
    chunk(fp,"IHDR",ihdr,13);
    chunk(fp,"IDAT",z,o-z);
    chunk(fp,"IEND",NULL,0);
  }
  free(raw);
  free(z);
  if(fp==NULL || fclose(fp)!=0)
    return -1;
  return 0;
}

//The average colour of a block of pixels
static void average(const struct picture *p, int x0, int x1, int y0, int y1, unsigned char out[3]){
  long sum[3]={0,0,0}, n=0;
  int x,y,i;
  unsigned char rgb[3];
  for(y=y0;y<y1;y++)
    for(x=x0;x<x1;x++){
      picture_rgb(p,x,y,rgb);
      for(i=0;i<3;i++)
        sum[i]+=rgb[i];
      n++;
    }
  for(i=0;i<3;i++)
    out[i]= n ? sum[i]/n : 0;
}

void print_picture(const struct picture *p, int columns){
  int col,row,rows;
  if(columns<1)
    columns=1;
  if(columns>PIC_W)
    columns=PIC_W;
  //Each half character cell is about square; a MODE 5 pixel is about
  //twice as wide as tall, so a half cell covers twice as many rows
  rows=(PIC_H*columns+4*PIC_W-1)/(4*PIC_W);
  for(row=0;row<rows;row++){
    for(col=0;col<columns;col++){
      int x0=col*PIC_W/columns, x1=(col+1)*PIC_W/columns;
      int ytop=(2*row)*PIC_H/(2*rows), ymid=(2*row+1)*PIC_H/(2*rows), ybot=(2*row+2)*PIC_H/(2*rows);
      unsigned char top[3], bottom[3];
      average(p,x0,x1,ytop,ymid,top);
      average(p,x0,x1,ymid,ybot,bottom);
      printf("\033[38;2;%d;%d;%dm\033[48;2;%d;%d;%dm\xe2\x96\x80",
             top[0],top[1],top[2],bottom[0],bottom[1],bottom[2]);
    }
    printf("\033[0m\n");
  }
}
