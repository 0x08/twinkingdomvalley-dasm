/* Pictures are drawn by running the game's own code in a 6502 emulator,
 * with enough of the BBC's VDU drivers for what it uses: MODE 5, palette
 * changes, GCOL and PLOT move/draw. Its fills write to screen memory. */
#ifndef PICTURES_H
#define PICTURES_H

#define PIC_W 160 //MODE 5 pixels, each about twice as wide as tall
#define PIC_H 256

struct picture {
  unsigned char pixel[PIC_H][PIC_W]; //Logical colours 0-3, top row first
  unsigned char palette[4];          //Physical colour 0-7 for each logical colour
};

//Ample for every picture; the ending's animated one ($C3) needs the most
#define PICTURE_MAXSTEPS 1000000000L

//Draws picture number n (as in the $1F00 table) using the game memory image
//(the 23,040 bytes from $0400). Returns 0, or -1 if the drawing code
//didn't finish within maxsteps instructions; what was drawn is kept.
int render_picture(const unsigned char *image, unsigned char n, struct picture *p, long maxsteps);

void picture_rgb(const struct picture *p, int x, int y, unsigned char rgb[3]);

//Writes the picture as a PNG, each pixel scale*2 wide and scale tall.
//Returns 0, or -1 if the file can't be written.
int write_png(const char *path, const struct picture *p, int scale);

//Prints the picture with 24-bit colour half-block characters, columns wide.
void print_picture(const struct picture *p, int columns);

#endif
