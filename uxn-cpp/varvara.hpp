#pragma once
#include "uxn.hpp"

namespace uxn {

static inline char inthex(int n) {
  n &= 0xf;
  return n < 10 ? '0' + n : 'a' + (n - 10);
}

template <typename T>
static inline T clamp(T v, T min, T max) {
  return v < min ? min : v > max ? max : v;
}

////////////////////////////////////////////////////////////
// CONSOLE

enum class ConsoleType : u8 {
  NoQueue = 0,
  Stdin = 1,
  Argument = 2,
  ArgumentSpacer = 3,
  ArgumentEnd = 4
};

class Console {
protected:
  Uxn& uxn;
public:
  Console(Uxn& uxn) : uxn(uxn) {}
  virtual ~Console() {}

  bool read_byte(u8 b, ConsoleType type) {
    uxn.dev[0x12] = b;
    uxn.dev[0x17] = static_cast<u8>(type);
    return uxn.call_vec(0x10);
  }
  bool read_args(int argc, char **argv);

  virtual void write_byte(u8 b) = 0;
};

////////////////////////////////////////////////////////////
// SCREEN

class Screen {
public:
  virtual ~Screen() {
    if (fg) delete[] fg;
    if (bg) delete[] bg;
  }

  u16 width() const { return w; }
  u16 height() const { return h; }

  virtual bool init() {
    update_palette();
    return true;
  }
  virtual void reset() {
    fill(bg, 0), fill(fg, 0);
    change(0, 0, w, h);
    update_palette();
  }
  virtual void update_palette() = 0;
  bool frame() {
    bool did_run = uxn.call_vec(0x20);
    if (dirty) {
      repaint();
      dirty = false;
    }
    return did_run;
  }
  virtual void repaint() = 0;
  virtual void try_resize(u16 width, u16 height);

  void before_dei(u8 d);
  void after_deo(u8 d);

protected:
  Uxn& uxn;
  u16 w, h;
  bool dirty = true;

  /* screen registers */
  u16 rX = 0, rY = 0, rA = 0, rMX = 0, rMY = 0, rMA = 0, rML = 0, rDX = 0, rDY = 0;

  Screen(Uxn& uxn, u16 width, u16 height) : uxn(uxn), w(width), h(height), fg(new u8[w*h]), bg(new u8[w*h]) {}
  virtual void on_resize() = 0;
  virtual void on_pixel(u16 x, u16 y, u8 color) = 0;

  void fill(u8 *layer, u8 color);
  void change(u16 x1, u16 y1, u16 x2, u16 y2);
  void redraw();

private:
  u16 screen_x1, screen_y1, screen_x2, screen_y2;
  u8 *fg, *bg;

  void rect(u8 *layer, u16 x1, u16 y1, u16 x2, u16 y2, u8 color);
  void sprite_2bpp(u8 *layer, const u8 *addr, u16 x1, u16 y1, u8 color, int fx, int fy);
  void sprite_1bpp(u8 *layer, const u8 *addr, u16 x1, u16 y1, u8 color, int fx, int fy);
  void draw_byte(u8 b, u16 x, u16 y, u8 color);
  void debugger();
};

class DummyScreen : public Screen {
public:
  DummyScreen(Uxn& uxn) : Screen(uxn, 320, 200) {}

  void reset() final {}
  void update_palette() final {}
  void repaint() final {}
  void try_resize(u16 width, u16 height) final {}
protected:
  void on_resize() final {}
  void on_pixel(u16 x, u16 y, u8 color) final {}
};

template <typename Pixel>
class PixelScreen : public Screen {
public:
  virtual ~PixelScreen() {
    delete[] pixels;
  }

  void update_palette() final {
    int i, shift;
    for (i = 0, shift = 4; i < 4; ++i, shift ^= 4) {
        u8 r = (uxn.dev[0x08 + i / 2] >> shift) & 0xf,
           g = (uxn.dev[0x0a + i / 2] >> shift) & 0xf,
           b = (uxn.dev[0x0c + i / 2] >> shift) & 0xf;
        palette[i] = color_from_12bit(r, g, b, i);
    }
    change(0, 0, w, h);
  }

  void repaint() final {
    redraw();
    on_paint();
  }

  void try_resize(u16 width, u16 height) {
    u16 old_w = w, old_h = h;
    Screen::try_resize(width, height);
    if (w != old_w || h != old_h) {
      delete[] pixels;
      pixels = new Pixel[w * h];
      on_resize();
      repaint();
    }
  }

protected:
  Pixel palette[4], *pixels;
  PixelScreen(Uxn& uxn, u16 width, u16 height) : Screen(uxn, width, height) {
    pixels = new Pixel[w * h];
  }
  virtual Pixel color_from_12bit(u8 r, u8 g, u8 b, u8 index) const = 0;
  virtual void on_paint() = 0;
  void on_pixel(u16 x, u16 y, u8 color) final {
    pixels[y*w+x] = palette[color];
  }
};

////////////////////////////////////////////////////////////
// AUDIO

static constexpr u8 AUDIO_VERSION = 1, POLYPHONY = 4;
static constexpr float AUDIO_BUFSIZE = 256, SAMPLE_FREQUENCY = 44100;

enum class EnvStage : u8 {
  Attack = (1 << 0),
  Decay = (1 << 1),
  Sustain = (1 << 2),
  Release = (1 << 3),
};

struct Envelope {
  float a, d, s, r, vol;
  EnvStage stage;

  void on();
  void off();
  void advance();
};

struct Sample {
  u8* data;
  float len, pos, inc, loop;
  u8 pitch;
  Envelope env;

  s16 next();
};

struct AudioChannel {
  Sample sample, next_sample;
  bool xfade;
  float duration, vol_l, vol_r;

  void note_on(float dur, u8 *data, u16 len, u8 vol, u8 attack, u8 decay, u8 sustain, u8 release, u8 pitch, bool loop);
  void note_off(float dur);
};

class Audio {
public:
  virtual ~Audio() {}
  virtual bool init() = 0;
  void write(u8* out_stream, size_t len);
  virtual void start(u8 instance);
  void before_dei(u8 d);
  void after_deo(u8 d);

  u8 get_vu(u8 instance) const {
    return channel[instance].sample.env.vol * 255.0f;
  }
  u8 get_position(u8 instance) const {
    return channel[instance].sample.pos;
  }
protected:
  Audio(Uxn& uxn) : uxn(uxn) {}
private:
  Uxn& uxn;
  AudioChannel channel[POLYPHONY];
};

class DummyAudio : public Audio {
public:
  DummyAudio(Uxn& uxn) : Audio(uxn) {}
  bool init() final { return true; }
  void start(u8 instance) final {}
};

////////////////////////////////////////////////////////////
// INPUT

enum class Button : u8 {
  None = 0,
  A = 0x01,
  B = 0x02,
  Select = 0x04,
  Start = 0x08,
  Up = 0x10,
  Down = 0x20,
  Left = 0x40,
  Right = 0x80
};

enum class MouseButton : u8 {
  None = 0,
  Left = 0x01,
  Right = 0x02,
  Middle = 0x04
};

static inline Button operator|(Button a, Button b) {
  return static_cast<Button>(static_cast<u8>(a) | static_cast<u8>(b));
}

class Input {
protected:
  static constexpr u8 player_offset[4] = { 0x82, 0x85, 0x86, 0x87 };
  Uxn& uxn;
public:
  Input(Uxn& uxn) : uxn(uxn) {}
  virtual ~Input() {}

  virtual bool key_down(u8 key) {
    uxn.dev[0x83] = key;
    return uxn.call_vec(0x80);
  }
  virtual bool key_up(u8 key) {
    if (uxn.dev[0x83] == key) uxn.dev[0x83] = 0;
    return false;
  }
  virtual bool button_down(Button button, u8 player = 0) {
    if (button == Button::None) return false;
    uxn.dev[player_offset[player % 4]] |= static_cast<u8>(button);
    uxn.dev[0x83] = 0;
    return uxn.call_vec(0x80);
  }
  virtual bool button_up(Button button, u8 player = 0) {
    if (button == Button::None) return false;
    uxn.dev[player_offset[player % 4]] &= ~static_cast<u8>(button);
    return uxn.call_vec(0x80);
  }
  virtual bool mouse_move(u16 x, u16 y) {
    poke2(uxn.dev + 0x92, x);
    poke2(uxn.dev + 0x94, y);
    return uxn.call_vec(0x90);
  }
  virtual bool mouse_down(MouseButton button) {
    if (button == MouseButton::None) return false;
    uxn.dev[0x96] |= static_cast<u8>(button);
    return uxn.call_vec(0x90);
  }
  virtual bool mouse_up(MouseButton button) {
    if (button == MouseButton::None) return false;
    uxn.dev[0x96] &= ~static_cast<u8>(button);
    return uxn.call_vec(0x90);
  }
  virtual bool mouse_scroll(u16 x, u16 y) {
    poke2(uxn.dev + 0x9a, static_cast<u16>(x));
    poke2(uxn.dev + 0x9c, static_cast<u16>(y));
    return uxn.call_vec(0x90);
  }
};

struct KeyMap {
  u8 a, b, select, start, up, down, left, right;

  Button button(u8 key) {
    if (key == a) return Button::A;
    if (key == b) return Button::B;
    if (key == select) return Button::Select;
    if (key == start) return Button::Start;
    if (key == up) return Button::Up;
    if (key == down) return Button::Down;
    if (key == left) return Button::Left;
    if (key == right) return Button::Right;
    return Button::None;
  }
};

class KeyMapInput : public Input {
protected:
  KeyMap key_map;
public:
  KeyMapInput(Uxn& uxn, KeyMap key_map) : Input(uxn), key_map(key_map) {}

  virtual bool key_down(u8 key) {
    uxn.dev[0x82] |= static_cast<u8>(key_map.button(key));
    return Input::key_down(key);
  }
  virtual bool key_up(u8 key) {
    bool button_up = false;
    if (u8 button = static_cast<u8>(key_map.button(key))) {
      uxn.dev[0x82] &= ~button;
      button_up = true;
    }
    Input::key_up(key);
    return button_up && uxn.call_vec(0x80);
  }
};

////////////////////////////////////////////////////////////
// FILESYSTEM

static constexpr u16 UXN_PATH_MAX = 4096;

enum class StatType : u8 {
  Unavailable,
  Directory,
  File,
  LargeFile
};

enum class ReadState : u8 {
  NotReading,
  ReadingFile,
  ReadingDirectory
};

struct Stat {
  StatType type;
  u16 size;
  const char* name;

  void write(MutableSlice out);
};

class Filesystem {
public:
  Filesystem(Uxn& uxn) : uxn(uxn) {}
  virtual ~Filesystem() {}

  virtual bool init() = 0;
  virtual const u8* load(const char* filename, size_t& out_size) = 0;

  void after_deo(u8 d);
protected:
  Uxn& uxn;
  char open_filename[UXN_PATH_MAX] = {0};

  virtual void close() = 0;
  virtual u16 read(MutableSlice dest) = 0;
  virtual Stat stat() = 0;
  virtual bool list_dir(Stat& out) = 0;
  virtual u16 write(Slice src, u8 append) = 0;
  virtual u16 remove() = 0;

private:
  char dir_entry[UXN_PATH_MAX + 6] = {0};
  u16 dir_entry_start = 0, dir_entry_end = 0;
  ReadState read_state;

  void write_dir_entry(Stat stat);
};

////////////////////////////////////////////////////////////
// DATETIME

class Datetime {
public:
  virtual ~Datetime() {}
  virtual u8 datetime_byte(u8 port) = 0;
};

////////////////////////////////////////////////////////////
// VARVARA

class Varvara : public Uxn {
public:
  virtual bool init();
  virtual void reset(bool soft);
  virtual void before_dei(u8 d);
  virtual void after_deo(u8 d);

protected:
  const char* boot_rom_filename;
  Console* base_console;
  Screen* base_screen;
  Audio* base_audio;
  Input* base_input;
  Filesystem* base_file;
  Datetime* base_datetime;

  Varvara(Console* console, Screen* screen, Audio* audio, Input* input, Filesystem* file, Datetime* time, const u8* rom, u32 rom_size)
  : Uxn(rom, rom_size),
    boot_rom_filename(nullptr),
    base_console(console),
    base_screen(screen),
    base_audio(audio),
    base_input(input),
    base_file(file),
    base_datetime(time) {}

  Varvara(Console* console, Screen* screen, Audio* audio, Input* input, Filesystem* file, Datetime* time, const char* rom_filename)
  : Uxn(nullptr, 0),
    boot_rom_filename(rom_filename),
    base_console(console),
    base_screen(screen),
    base_audio(audio),
    base_input(input),
    base_file(file),
    base_datetime(time) {}

  virtual void on_system_debug(u8 b) {}
};

}
