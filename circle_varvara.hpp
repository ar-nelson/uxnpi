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
#include <circle/sound/soundbasedevice.h>
#include <circle/types.h>
#include <fatfs/ff.h>

#include "uxn-cpp/varvara.hpp"
#include "safe_shutdown.hpp"

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
  u16 offset_x, offset_y;
  u8 zoom;
public:
  CircleScreen(Uxn& uxn, C2DGraphics& gfx) :
    PixelScreen<TScreenColor>(uxn, gfx.GetWidth(), gfx.GetHeight()),
    gfx(gfx), offset_x(0), offset_y(0), zoom(1) {}
  void try_resize(u16 width, u16 height) final {
    auto max_w = gfx.GetWidth(), max_h = gfx.GetHeight();
    PixelScreen::try_resize(width > max_w ? max_w : width, height > max_h ? max_h : height);
  }
  void on_resize() final {
    auto max_w = gfx.GetWidth(), max_h = gfx.GetHeight();
    for (zoom = 1; w * zoom <= max_w && h * zoom <= max_h; zoom += 1) {}
    if (zoom > 1) zoom -= 1;
    offset_x = (max_w - w * zoom) / 2, offset_y = (max_h - h * zoom) / 2;
  }
protected:
  TScreenColor color_from_12bit(u8 r, u8 g, u8 b, u8 index) const final {
    return COLOR16(r*2, g*2, b*2);
  }
  void on_paint() final {
    if (offset_x || offset_y) gfx.ClearScreen(palette[0]);
    if (zoom <= 1) {
      gfx.DrawImage(offset_x, offset_y, w, h, const_cast<TScreenColor*>(pixels));
    } else {
      auto max_w = gfx.GetWidth();
      TScreenColor* buf = gfx.GetBuffer();
      for (unsigned x = 0; x < w; x++) {
        for (unsigned y = 0; y < h; y++) {
          unsigned pixel_ix = y * w + x;
          for (unsigned xx = offset_x + x * zoom; xx < offset_x + (x+1) * zoom; xx++) {
            for (unsigned yy = offset_y + y * zoom; yy < offset_y + (y+1) * zoom; yy++) {
              buf[yy * max_w + xx] = pixels[pixel_ix];
            }
          }
        }
      }
    }
    gfx.UpdateDisplay();
  }
};

class CircleAudio : public Audio {
  static constexpr size_t BUFSIZE = static_cast<size_t>(AUDIO_BUFSIZE);
  static void need_data_callback(void* user_data) {
    CircleAudio* self = reinterpret_cast<CircleAudio*>(user_data);
    u8 buf[BUFSIZE];
    self->write(buf, BUFSIZE);
    self->device->Write(buf, BUFSIZE);
  }

  CSoundBaseDevice* device;

public:
  CircleAudio(Uxn& uxn, CSoundBaseDevice* device) : Audio(uxn), device(device) {}

  bool init() final {
    // if we have no audio device, don't bother
    if (!device) return true;

    device->SetWriteFormat(TSoundFormat::SoundFormatSigned16, 2);
    device->RegisterNeedDataCallback(need_data_callback, this);

    // Allocate a queue of AUDIO_BUFSIZE 16-bit frames.
    // AUDIO_BUFSIZE is actually in bytes, so this is twice the buffer size.
    // This is intentional: according to Circle's documentation,
    // RegisterNeedDataCallback's callback is called when at least half
    // of the queue is empty, so we can always safely write the full buffer.
    return device->AllocateQueueFrames(BUFSIZE);
  }

  void start(u8 instance) final {
    if (!device) return;
    Audio::start(instance);
    if (!device->IsActive()) device->Start();
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
  CircleAudio audio;
  Input input;
  CircleFilesystem file;
  CircleDatetime datetime;
  C2DGraphics& gfx;
  CTimer& timer;
public:
  CircleVarvara(
    C2DGraphics& gfx,
    CSoundBaseDevice* sound,
    CTimer& t,
    CLogger& logger,
    FATFS& fs,
    const char* rom_filename = "boot.rom"
  ) : console(*this, logger),
      screen(*this, gfx),
      audio(*this, sound),
      input(*this),
      file(*this, fs, logger),
      datetime(t),
      Varvara(&console, &screen, &audio, &input, &file, &datetime, rom_filename),
      gfx(gfx),
      timer(t) {}

  void game_pad_input(const TGamePadState* state);
  ShutdownMode run(SafeShutdown* safe_shutdown = nullptr);
};

}
