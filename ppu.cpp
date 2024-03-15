#include "ppu.hpp"
#include <circle/util.h>

static u8 blending[5][16] = {
  {0, 0, 0, 0, 1, 0, 1, 1, 2, 2, 0, 2, 3, 3, 3, 0},
  {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3},
  {1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1},
  {2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2},
  {1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0}
};

Ppu::Ppu(CScreenDevice* screen) : screen(screen),
  width(static_cast<u16>(screen->GetWidth())), height(static_cast<u16>(screen->GetHeight())),
  pixels(new u8[width * height]), dirty_lines(new u8[height]),
  color_bytes({0x4f, 0x7f, 0x4f, 0x77, 0x4f, 0xf7})
{
  update_colors();

  // Clear pixel buffer memory.
  memset(pixels, 0, sizeof(pixels));

  // Make sure we perform an initial screen drawing.
  reqdraw = 1;
  redraw_screen();
}

Ppu::~Ppu() {
  delete pixels;
  delete dirty_lines;
}

void Ppu::pixel(u8 layer, u16 x, u16 y, u8 color) {
  size_t idx = y * width + x;
  u8 *pixel = &pixels[idx], shift = layer * 2;
  if(x < width && y < height) {
    *pixel = (*pixel & ~(0x3 << shift)) | (color << shift);
  }
  dirty_lines[y] |= 1;
  reqdraw = 1;
}

void Ppu::sprite_1bpp(u8 layer, u16 x, u16 y, u8 *sprite, u8 color, u8 flipx, u8 flipy) {
  for (u16 v = 0; v < 8; v++) {
    for (u16 h = 0; h < 8; h++) {
      u8 ch1 = (sprite[v] >> (7 - h)) & 0x1;
      if (ch1 || blending[4][color]) {
        pixel(
          layer,
          x + (flipx ? 7 - h : h),
          y + (flipy ? 7 - v : v),
          blending[ch1][color]
        );
      }
    }
  }
  reqdraw = 1;
}

void Ppu::sprite_2bpp(u8 layer, u16 x, u16 y, u8 *sprite, u8 color, u8 flipx, u8 flipy) {
  for (u16 v = 0; v < 8; v++) {
    for (u16 h = 0; h < 8; h++) {
      u8 ch1 = ((sprite[v] >> (7 - h)) & 0x1);
      u8 ch2 = ((sprite[v + 8] >> (7 - h)) & 0x1);
      u8 ch = ch1 + ch2 * 2;
      if (ch || blending[4][color]) {
        pixel(
          layer,
          x + (flipx ? 7 - h : h),
          y + (flipy ? 7 - v : v),
          blending[ch][color]
        );
      }
    }
  }
  reqdraw = 1;
}

void Ppu::redraw_screen() {
  for (u16 j = 0; j < height; j++) {
    dirty_lines[j] = 1;
  }
}

void Ppu::blit() {
  if (reqdraw == 0) return;
  for (size_t j = 0; j < height; j++) {
    if (dirty_lines[j] != 0) {
      for (size_t i = 0; i < width; i++) {
        size_t idx = i + j * width;
        screen->SetPixel(i, j, palette[pixels[idx] % 4]);
      }
    }
    dirty_lines[j] = 0;
  }
  reqdraw = 0;
}

void Ppu::update_colors() {
  for (size_t i = 0; i < 4; ++i) {
    u8 r = (color_bytes[i / 2] >> (!(i % 2) << 2)) & 0x0f;
    u8 g = (color_bytes[2 + i / 2] >> (!(i % 2) << 2)) & 0x0f;
    u8 b = (color_bytes[4 + i / 2] >> (!(i % 2) << 2)) & 0x0f;

    palette[i] = COLOR16(r, g, b);
  }

  reqdraw = 1;
  redraw_screen();
}
