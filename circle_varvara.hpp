#pragma once

#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/2dgraphics.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/time.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbgamepad.h>
#include <circle/types.h>
#include <fatfs/ff.h>

#include "uxn-cpp/varvara.hpp"

namespace uxn {

class CircleConsole : public Console {
  CLogger& logger;
  char buf[1024];
  u16 buf_sz;
public:
  CircleConsole(Uxn& uxn, CLogger& logger) : Console(uxn), logger(logger), buf_sz(0) {}
  void write_byte(u8 b) final;
  void flush();
};

class CircleScreen : public PixelScreen<TScreenColor> {
  C2DGraphics& gfx;
  u16 offset_x, offset_y, max_w, max_h;
public:
  CircleScreen(Uxn& uxn, C2DGraphics& gfx) :
    PixelScreen<TScreenColor>(uxn, gfx.GetWidth(), gfx.GetHeight()),
    gfx(gfx), offset_x(0), offset_y(0), max_w(gfx.GetWidth()), max_h(gfx.GetHeight()) {}
  void try_resize(u16 width, u16 height) final {
    PixelScreen::try_resize(width > max_w ? max_w : width, height > max_h ? max_h : height);
  }
  void on_resize() final {
    max_w = gfx.GetWidth(), max_h = gfx.GetHeight();
    /*
    unsigned sw, sh;
    for (sw = max_w, sh = max_h; sw > w && sh > h; sw /= 2, sh /= 2) {}
    if (sw < max_w) {
    retry:
      if (!gfx.Resize(sw, sh)) {
        if (sw >= max_w) {
          LOGMODULE("Screen");
          LOGERR("Resize failed");
        } else {
          sw *= 2, sh *= 2;
          goto retry;
        }
      }
    }
    */
    offset_x = (max_w - w) / 2, offset_y = (max_h - h) / 2;
  }
protected:
  TScreenColor color_from_12bit(u8 r, u8 g, u8 b, u8 index) const final {
    return COLOR16(r*2, g*2, b*2);
  }
  void on_paint(const TScreenColor* pixels) final {
    if (offset_x || offset_y) gfx.ClearScreen(palette[0]);
    gfx.DrawImage(offset_x, offset_y, w, h, const_cast<TScreenColor*>(pixels));
    gfx.UpdateDisplay();
  }
};

class CircleFilesystem : public Filesystem {
  FATFS& fs;
  CLogger& logger;

  enum class OpenState : u8 { None, ReadFile, ReadDir, Write, Append };
  union { FIL fil; DIR dir; } open_file;
  OpenState open_state = OpenState::None;
  FILINFO last_filinfo;
  u8* loaded_rom = nullptr;
  u32 loaded_rom_capacity = 0;
  char absolute_buffer[UXN_PATH_MAX] = {0};
  const char* absolute_filename(const char* relative);
public:
  CircleFilesystem(Uxn& uxn, FATFS& fs, CLogger& logger) : Filesystem(uxn), fs(fs), logger(logger) {}
  virtual ~CircleFilesystem() { close(); if (loaded_rom) delete[] loaded_rom; }
  bool init() final { return true; }
  const u8* load(const char* filename, size_t& out_size) final;
protected:
  void close() final;
  u16 read(MutableSlice dest) final;
  Stat stat() final;
  bool list_dir(Stat& out) final;
  u16 write(Slice src, u8 append) final;
  u16 remove() final;
};

class CircleDatetime : public Datetime {
  CTimer& timer;
public:
  CircleDatetime(CTimer& timer) : timer(timer) {}
  u8 datetime_byte(u8 port) final;
};

class CircleVarvara : public Varvara {
  CircleConsole console;
  CircleScreen screen;
  Input input;
  CircleFilesystem file;
  CircleDatetime datetime;
  C2DGraphics& gfx;
  CTimer& timer;
public:
  CircleVarvara(
    C2DGraphics& gfx,
    CTimer& t,
    CLogger& logger,
    FATFS& fs,
    const char* rom_filename = "boot.rom"
  ) : console(*this, logger),
      screen(*this, gfx),
      input(*this),
      file(*this, fs, logger),
      datetime(t),
      Varvara(&console, &screen, &input, &file, &datetime, rom_filename),
      gfx(gfx),
      timer(t) {}

  void game_pad_input(const TGamePadState* state);
  void run();
};

}
