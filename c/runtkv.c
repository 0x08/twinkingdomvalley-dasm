/* runtkv: plays the original Twin Kingdom Valley by running its 6502 code
 * in the emulator, with just enough of the BBC Micro's operating system:
 * text output in MODE 7 (teletext colours become ANSI colours on a
 * terminal), line input, single keys and the clock. Input lines are read
 * from stdin, so scripted games can be compared with playtkv. Pictures
 * are drawn in MODE 5 by the game and printed when it returns to MODE 7.
 *
 * Usage: runtkv [-w]
 *   -w  don't wrap lines at 40 characters as the BBC's screen does; the
 *       game still breaks its own lines between words
 * TKV_IMAGE chooses the game image as for the other tools, and
 * TKV_PICTURE_WIDTH the width of pictures. *SAVE and *LOAD do nothing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libtkv.h"
#include "m6502.h"
#include "pictures.h"

static struct m6502 cpu;
static int colour_output;  //stdout is a terminal
static int column;         //Text column, 0-39
static int hidden;         //Between MODE and VDU 28: the "Press TAB" banner
static unsigned long clock_cs;  //Centiseconds, advanced on every read
static int vdu_needed;
static int screen_wrap=1;  //Start a new line after 40 characters
static struct vdu *graphics; //The VDU driver while in MODE 5, drawing a picture

//Bytes following each VDU code, see the BBC Micro User Guide
static const unsigned char vdu_params[32]={
  0,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,1,2,5,0,0,1,9, 8,5,0,0,4,4,0,2};

static void newline(){
  if(colour_output)
    fputs("\033[0m",stdout);//Teletext colours last to the end of a line
  putchar('\n');
  column=0;
}

//The game switches to MODE 5 to draw a picture and back to MODE 7 when
//it's done, when the picture is still in screen memory
static void change_mode(unsigned char mode){
  if(mode==5){
    if(graphics==NULL)
      graphics=vdu_create(&cpu);
    vdu_write(graphics,22);
    vdu_write(graphics,5);
  } else if(graphics!=NULL){
    static struct picture pic;
    vdu_read_screen(graphics,&pic);
    free(graphics);
    graphics=NULL;
    if(column>0)
      newline();
    fflush(stdout);
    if(getenv("TKV_PICTURE_HASH"))//For comparing with the browser version
      printf("[picture:%08lx]\n",picture_crc(&pic));
    else
      print_picture(&pic,picture_columns());
  }
}

//OSWRCH: MODE 7 text, or MODE 5 graphics for the VDU driver
static void oswrch(unsigned char c){
  static unsigned char command;
  if(vdu_needed>0){//Parameters of a VDU command
    if(graphics!=NULL && !(command==22))
      vdu_write(graphics,c);
    if(--vdu_needed==0){
      if(command==28)
        hidden=0;
      else if(command==22)
        change_mode(c);
    }
    return;
  }
  if(graphics!=NULL){//Drawing a picture: everything goes to the VDU driver
    if(c<32){
      command=c;
      vdu_needed=vdu_params[c];
      if(c!=22)
        vdu_write(graphics,c);
    } else
      vdu_write(graphics,c);
    return;
  }
  if(c<32 || c==127){
    command=c;
    vdu_needed = c<32 ? vdu_params[c] : 0;
    if(c==22)//MODE clears the screen; the banner is printed before the text window
      hidden=1;
    if(hidden)
      return;
    if(c==10)
      newline();
    else if(c==13)
      ;//The cursor goes back to the start of the line; LF does the work here
    else if(c==8 && column>0){
      fputs("\b",stdout);
      column--;
    }
    return;
  }
  if(hidden)
    return;
  if(c>=128 && c<160){//A teletext control code shows as a space
    if(colour_output && c>=129 && c<=135){
      static const int ansi[8]={0,31,32,33,34,35,36,37};
      printf("\033[%dm",ansi[c-128]);
    }
    putchar(' ');
  } else {
    c = c>=160 ? c-128 : c;
    putchar(c=='_' ? '-' : c);//MODE 7 shows $5F as a horizontal bar
  }
  if(++column==40 && screen_wrap)//The screen is 40 characters wide
    newline();
}

static void halt(){
  if(column>0)
    newline();
  cpu.halt=1;
}

//OSWORD 0: read a line into the buffer given in the parameter block
static void read_line(unsigned short block){
  char line[256];
  unsigned short buf=cpu.mem[block]|(cpu.mem[block+1]<<8);
  int max=cpu.mem[block+2], n;
  fflush(stdout);
  if(fgets(line,sizeof(line),stdin)==NULL){
    halt();
    return;
  }
  line[strcspn(line,"\r\n")]='\0';
  n=strlen(line);
  if(n>max)
    n=max;
  memcpy(cpu.mem+buf,line,n);
  cpu.mem[buf+n]=13;
  cpu.y=n;
  cpu.p&=~1;//Carry clear: not Escape
  column=0;//The line typed, and its Return, are on the terminal
}

//OSRDCH: the next key. Input is read in lines, so the rest of the line goes.
static void read_key(){
  int c,k;
  fflush(stdout);
  c=getchar();
  if(c==EOF){
    halt();
    return;
  }
  k=c;
  while(c!='\n' && c!=EOF)
    c=getchar();
  cpu.a = k=='\n' ? 13 : k;
  cpu.p&=~1;
}

static void os_call(struct m6502 *c, unsigned short address){
  switch(address){
    case 0xFFEE: oswrch(c->a); break;//OSWRCH
    case 0xFFE3://OSASCI
      if(c->a==13){ oswrch(10); oswrch(13); } else oswrch(c->a);
      break;
    case 0xFFE7: oswrch(10); oswrch(13); break;//OSNEWL
    case 0xFFE0: read_key(); break;//OSRDCH
    case 0xFFF1: {//OSWORD
      unsigned short block=c->x|(c->y<<8);
      if(c->a==0)
        read_line(block);
      else if(c->a==1){//Read the clock: five bytes, least significant first
        int i;
        clock_cs++;
        for(i=0;i<5;i++)
          c->mem[block+i]= i<4 ? (clock_cs>>(8*i))&0xFF : 0;
      }
      break;
    }
    case 0xFFF4://OSBYTE
      if(c->a==0x80)//Characters waiting: always say there's input
        c->x=c->y=1;
      else if(c->a==0x81)//INKEY: no key held, eg TAB for a picture
        c->x=c->y=0;
      break;
    //OSFILE (*SAVE, *LOAD) and anything else: nothing happens
  }
}

int main(int argc, char *argv[]){
  if(argc>1 && strcmp(argv[1],"-w")==0)
    screen_wrap=0;
  init_tkv();
  colour_output=isatty(STDOUT_FILENO);
  memcpy(cpu.mem+0x0400,ctkv,FILESIZE);
  cpu.os_call=os_call;
  cpu.s=0xFF;
  m6502_call(&cpu,0x1400,0x7FFFFFFFFFFFFFFFL);//Where LVAL starts the game
  if(colour_output)
    fputs("\033[0m",stdout);
  if(column>0)
    putchar('\n');
  return 0;
}
