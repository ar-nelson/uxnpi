#pragma once
#include <circle/screen.h>
#include "shorthand.h"

class Ppu {
private:
  CScreenDevice* screen;
public:
  u16 width, height;
private:
  u8 *pixels, *dirty_lines, reqdraw = 0, color_bytes[6];
  TScreenColor palette[4];
  void redraw_screen();
  void update_colors();
public:
  Ppu(CScreenDevice* screen);
  ~Ppu();
  void pixel(u8 layer, u16 x, u16 y, u8 color);
  void sprite_1bpp(u8 layer, u16 x, u16 y, u8 *sprite, u8 color, u8 flipx, u8 flipy);
  void sprite_2bpp(u8 layer, u16 x, u16 y, u8 *sprite, u8 color, u8 flipx, u8 flipy);
  void blit();

  void set_color_byte(u8 offset, u8 byte) {
    color_bytes[offset % 6] = byte;
    update_colors();
  }
};
