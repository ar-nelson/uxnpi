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
  bool read_args(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
      if (i) if (!read_byte(' ', ConsoleType::ArgumentSpacer)) return false;
      for (size_t c = 0; argv[i][c] != '\0'; c++) {
        if (!read_byte(argv[i][c], ConsoleType::Argument)) return false;
      }
    }
    return read_byte(0, ConsoleType::ArgumentEnd);
  }

  virtual void write_byte(u8 b) = 0;
};

////////////////////////////////////////////////////////////
// SCREEN

class Screen {
public:
  virtual ~Screen() {}

  u16 width() const { return w; }
  u16 height() const { return h; }

  virtual bool init() {
    update_palette();
    return true;
  }
  virtual void reset() = 0;
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
  virtual void try_resize(u16 width, u16 height) = 0;

  void before_dei(u8 d) {
    u8& b = uxn.dev[d];
    switch (d) {
      case 0x22: b = w >> 8; return;
      case 0x23: b = w; return;
      case 0x24: b = h >> 8; return;
      case 0x25: b = h; return;
      case 0x28: b = rX >> 8; return;
      case 0x29: b = rX; return;
      case 0x2a: b = rY >> 8; return;
      case 0x2b: b = rY; return;
      case 0x2c: b = rA >> 8; return;
      case 0x2d: b = rA; return;
    }
  }
  virtual void after_deo(u8 d) = 0;

protected:
  Uxn& uxn;
  u16 w, h;
  bool dirty = true;

  /* screen registers */
  u16 rX = 0, rY = 0, rA = 0, rMX = 0, rMY = 0, rMA = 0, rML = 0, rDX = 0, rDY = 0;

  Screen(Uxn& uxn, u16 width, u16 height) : uxn(uxn), w(width), h(height) {}
  virtual void on_resize() = 0;
};

class DummyScreen : public Screen {
public:
  DummyScreen(Uxn& uxn) : Screen(uxn, 320, 200) {}

  void reset() final {}
  void update_palette() final {}
  void repaint() final {}
  void try_resize(u16 width, u16 height) final {}
  void after_deo(u8 d) final {
    u8* dev = uxn.dev;
    switch (d) {
      case 0x26: rMX = dev[0x26] & 0x1, rMY = dev[0x26] & 0x2, rMA = dev[0x26] & 0x4, rML = dev[0x26] >> 4, rDX = rMX << 3, rDY = rMY << 2; return;
      case 0x28:
      case 0x29: rX = (dev[0x28] << 8) | dev[0x29]; return;
      case 0x2a:
      case 0x2b: rY = (dev[0x2a] << 8) | dev[0x2b]; return;
      case 0x2c:
      case 0x2d: rA = (dev[0x2c] << 8) | dev[0x2d]; return;
    }
  }
protected:
  void on_resize() final {}
};

template <typename Pixel>
class PixelScreen : public Screen {
public:
  virtual ~PixelScreen() {
    delete[] bg;
    delete[] fg;
    delete[] pixels;
  }

  virtual void reset() {
    fill(bg, 0), fill(fg, 0);
    change(0, 0, w, h);
    update_palette();
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
    on_paint(pixels);
  }
  void try_resize(u16 width, u16 height) {
    if (width < 0x8 || height < 0x8 || width >= 0x800 || height >= 0x800)
      return;
    if (w == width && h == height)
      return;
    delete[] bg;
    delete[] fg;
    delete[] pixels;
    bg = new u8[width * height], fg = new u8[width * height], pixels = new Pixel[width * height];
    w = width, h = height;
    fill(bg, 0), fill(fg, 0);
    change(0, 0, width, height);
    on_resize();
    on_paint(pixels);
    dirty = false;
  }

  void after_deo(u8 d) final {
    u8* dev = uxn.dev;
    switch (d) {
      case 0x23: try_resize(peek2(dev + 0x22), h); return;
      case 0x25: try_resize(w, peek2(dev + 0x24)); return;
      case 0x26: rMX = dev[0x26] & 0x1, rMY = dev[0x26] & 0x2, rMA = dev[0x26] & 0x4, rML = dev[0x26] >> 4, rDX = rMX << 3, rDY = rMY << 2; return;
      case 0x28:
      case 0x29: rX = (dev[0x28] << 8) | dev[0x29]; return;
      case 0x2a:
      case 0x2b: rY = (dev[0x2a] << 8) | dev[0x2b]; return;
      case 0x2c:
      case 0x2d: rA = (dev[0x2c] << 8) | dev[0x2d]; return;
      case 0x2e: {
        u8 ctrl = dev[0x2e];
        u8 color = ctrl & 0x3;
        u8 *layer = ctrl & 0x40 ? fg : bg;
        /* fill mode */
        if (ctrl & 0x80) {
          u16 x1, y1, x2, y2;
          if (ctrl & 0x10)
            x1 = 0, x2 = rX;
          else
            x1 = rX, x2 = w;
          if (ctrl & 0x20)
            y1 = 0, y2 = rY;
          else
            y1 = rY, y2 = h;
          rect(layer, x1, y1, x2, y2, color);
          change(x1, y1, x2, y2);
        }
        /* pixel mode */
        else {
          if (rX < w && rY < h)
            layer[rX + rY * w] = color;
          change(rX, rY, rX + 1, rY + 1);
          if (rMX) rX++;
          if (rMY) rY++;
        }
        return;
      }
      case 0x2f: {
        u8 i;
        u8 ctrl = dev[0x2f];
        u8 twobpp = !!(ctrl & 0x80);
        u8 color = ctrl & 0xf;
        u8 *layer = ctrl & 0x40 ? fg : bg;
        int fx = ctrl & 0x10 ? -1 : 1;
        int fy = ctrl & 0x20 ? -1 : 1;
        u16 dxy = rDX * fy, dyx = rDY * fx, addr_incr = rMA << (1 + twobpp);
        if (twobpp)
          for (i = 0; i <= rML; i++, rA += addr_incr)
            sprite_2bpp(layer, &uxn.ram[rA], rX + dyx * i, rY + dxy * i, color, fx, fy);
        else
          for (i = 0; i <= rML; i++, rA += addr_incr)
            sprite_1bpp(layer, &uxn.ram[rA], rX + dyx * i, rY + dxy * i, color, fx, fy);
        change(rX, rY, rX + dyx * rML + 8, rY + dxy * rML + 8);
        if (rMX) rX += rDX * fx;
        if (rMY) rY += rDY * fy;
        return;
      }
    }
  }

protected:
  Pixel palette[4];
  PixelScreen(Uxn& uxn, u16 width, u16 height) : Screen(uxn, width, height) {
    bg = new u8[w * h];
    fg = new u8[w * h];
    pixels = new Pixel[w * h];
  }
  virtual Pixel color_from_12bit(u8 r, u8 g, u8 b, u8 index) const = 0;
  virtual void on_paint(const Pixel* pixels) = 0;

private:
  /* c = !ch ? (color % 5 ? color >> 2 : 0) : color % 4 + ch == 1 ? 0 : (ch - 2 + (color & 3)) % 3 + 1; */

  static constexpr u8
    blending[][16] = {
      {0, 0, 0, 0, 1, 0, 1, 1, 2, 2, 0, 2, 3, 3, 3, 0},
      {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3},
      {1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1},
      {2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2},
      {0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0}
    },
    icons[] = {
      0x00, 0x7c, 0x82, 0x82, 0x82, 0x82, 0x82, 0x7c, 0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10,
      0x10, 0x00, 0x7c, 0x82, 0x02, 0x7c, 0x80, 0x80, 0xfe, 0x00, 0x7c, 0x82, 0x02, 0x1c, 0x02,
      0x82, 0x7c, 0x00, 0x0c, 0x14, 0x24, 0x44, 0x84, 0xfe, 0x04, 0x00, 0xfe, 0x80, 0x80, 0x7c,
      0x02, 0x82, 0x7c, 0x00, 0x7c, 0x82, 0x80, 0xfc, 0x82, 0x82, 0x7c, 0x00, 0x7c, 0x82, 0x02,
      0x1e, 0x02, 0x02, 0x02, 0x00, 0x7c, 0x82, 0x82, 0x7c, 0x82, 0x82, 0x7c, 0x00, 0x7c, 0x82,
      0x82, 0x7e, 0x02, 0x82, 0x7c, 0x00, 0x7c, 0x82, 0x02, 0x7e, 0x82, 0x82, 0x7e, 0x00, 0xfc,
      0x82, 0x82, 0xfc, 0x82, 0x82, 0xfc, 0x00, 0x7c, 0x82, 0x80, 0x80, 0x80, 0x82, 0x7c, 0x00,
      0xfc, 0x82, 0x82, 0x82, 0x82, 0x82, 0xfc, 0x00, 0x7c, 0x82, 0x80, 0xf0, 0x80, 0x82, 0x7c,
      0x00, 0x7c, 0x82, 0x80, 0xf0, 0x80, 0x80, 0x80
    },
    arrow[] = {
      0x00, 0x00, 0x00, 0xfe, 0x7c, 0x38, 0x10, 0x00
    };

  u16 screen_x1, screen_y1, screen_x2, screen_y2;
  u8 *fg, *bg;
  Pixel* pixels;

  void change(u16 x1, u16 y1, u16 x2, u16 y2) {
    if (x1 > w && x2 > x1) return;
    if (y1 > h && y2 > y1) return;
    if (x1 > x2) x1 = 0;
    if (y1 > y2) y1 = 0;
    if (x1 < screen_x1) screen_x1 = x1;
    if (y1 < screen_y1) screen_y1 = y1;
    if (x2 > screen_x2) screen_x2 = x2;
    if (y2 > screen_y2) screen_y2 = y2;
    dirty = true;
  }

  void fill(u8 *layer, u8 color) {
    size_t i, length = w * h;
    for (i = 0; i < length; i++)
      layer[i] = color;
    dirty = true;
  }

  void rect(u8 *layer, u16 x1, u16 y1, u16 x2, u16 y2, u8 color) {
    size_t row, x, y;
    for (y = y1; y < y2 && y < h; y++)
      for (x = x1, row = y * w; x < x2 && x < w; x++)
        layer[x + row] = color;
    dirty = true;
  }

  void sprite_2bpp(u8 *layer, const u8 *addr, u16 x1, u16 y1, u8 color, int fx, int fy) {
    u8 opaque = blending[4][color];
    u16 y, ymod = (fy < 0 ? 7 : 0), ymax = y1 + ymod + fy * 8;
    u16 x, xmod = (fx > 0 ? 7 : 0), xmax = x1 + xmod - fx * 8;
    for (y = y1 + ymod; y != ymax; y += fy, addr++) {
      int c = addr[0] | (addr[8] << 8), row = y * w;
      if (y < h)
        for (x = x1 + xmod; x != xmax; x -= fx, c >>= 1) {
          u8 ch = (c & 1) | ((c >> 7) & 2);
          if (x < w && (opaque || ch))
            layer[x + row] = blending[ch][color];
        }
    }
    dirty = true;
  }

  void sprite_1bpp(u8 *layer, const u8 *addr, u16 x1, u16 y1, u8 color, int fx, int fy) {
    u8 opaque = blending[4][color];
    u16 y, ymod = (fy < 0 ? 7 : 0), ymax = y1 + ymod + fy * 8;
    u16 x, xmod = (fx > 0 ? 7 : 0), xmax = x1 + xmod - fx * 8;
    for (y = y1 + ymod; y != ymax; y += fy) {
      int c = *addr++, row = y * w;
      if (y < h)
        for (x = x1 + xmod; x != xmax; x -= fx, c >>= 1) {
          u8 ch = c & 1;
          if (x < w && (opaque || ch))
            layer[x + row] = blending[ch][color];
        }
    }
    dirty = true;
  }

  void draw_byte(u8 b, u16 x, u16 y, u8 color) {
    sprite_1bpp(fg, &icons[(b >> 4) << 3], x, y, color, 1, 1);
    sprite_1bpp(fg, &icons[(b & 0xf) << 3], x + 8, y, color, 1, 1);
    change(x, y, x + 0x10, y + 0x8);
  }

  void debugger() {
    int i;
    for(i = 0; i < 0x08; i++) {
      u8 pos = uxn.wst.ptr - 4 + i;
      u8 color = i > 4 ? 0x01 : !pos ? 0xc : i == 4 ? 0x8 : 0x2;
      draw_byte(uxn.wst.dat[pos], i * 0x18 + 0x8, h - 0x18, color);
    }
    for(i = 0; i < 0x08; i++) {
      u8 pos = uxn.rst.ptr - 4 + i;
      u8 color = i > 4 ? 0x01 : !pos ? 0xc : i == 4 ? 0x8 : 0x2;
      draw_byte(uxn.rst.dat[pos], i * 0x18 + 0x8, h - 0x10, color);
    }
    sprite_1bpp(fg, &arrow[0], 0x68, h - 0x20, 3, 1, 1);
    for(i = 0; i < 0x20; i++)
      draw_byte(uxn.ram[i], (i & 0x7) * 0x18 + 0x8, ((i >> 3) << 3) + 0x8, 1 + !!uxn.ram[i]);
  }

  void redraw() {
    int i, j, o, y;
    u16 x1 = screen_x1, y1 = screen_y1;
    u16 x2 = screen_x2 > w ? w : screen_x2, y2 = screen_y2 > h ? h : screen_y2;
    Pixel _palette[16];
    screen_x1 = screen_y1 = 0xffff;
    screen_x2 = screen_y2 = 0;
    if (uxn.dev[0x0e])
      debugger();
    for (i = 0; i < 16; i++)
      _palette[i] = palette[(i >> 2) ? (i >> 2) : (i & 3)];
    for (y = y1; y < y2; y++)
      for (o = y * w, i = x1 + o, j = x2 + o; i < j; i++)
        pixels[i] = _palette[fg[i] << 2 | bg[i]];
  }
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

  void write(MutableSlice out) {
    u16 i;
    if (!out.size) return;
    switch (type) {
      case StatType::Unavailable:
        for (i = 0; i < out.size; i++)
          out[i] = '!';
        break;
      case StatType::Directory:
        for (i = 0; i < out.size; i++)
          out[i] = '-';
        break;
      case StatType::File:
        for (i = 0; i < out.size; i++)
          out[i] = inthex(size >> ((out.size - i - 1) << 2));
        break;
      case StatType::LargeFile:
        for (i = 0; i < out.size; i++)
          out[i] = '?';
        break;
    }
  }
};

class Filesystem {
public:
  Filesystem(Uxn& uxn) : uxn(uxn) {}
  virtual ~Filesystem() {}

  virtual bool init() = 0;
  virtual const u8* load(const char* filename, size_t& out_size) = 0;

  void after_deo(u8 d) {
    u8* dev = uxn.dev;
    switch (d) {
      // File
      case 0xa5: {
        read_state = ReadState::NotReading;
        dir_entry_end = dir_entry_start = 0;
        u16 len = peek2(dev + 0xaa);
        stat().write(uxn.bounded_range_in_ram_mutable(peek2(dev + 0xa4), len));
        poke2(dev + 0xa2, len);
        return;
      }
      case 0xa6:
        read_state = ReadState::NotReading;
        dir_entry_end = dir_entry_start = 0;
        poke2(dev + 0xa2, remove());
        return;
      case 0xa9: {
        close();
        read_state = ReadState::NotReading;
        dir_entry_end = dir_entry_start = 0;
        auto name = uxn.null_terminated_string_in_ram(peek2(dev + 0xa8));
        u16 max = name.size;
        if (max > UXN_PATH_MAX) max = UXN_PATH_MAX;
        for (u16 i = 0; i < max; i++) open_filename[i] = name[i];
        open_filename[max] = '\0';
        poke2(dev + 0xa2, 1);
        return;
      }
      case 0xad: {
        if (read_state == ReadState::NotReading) {
          auto st = stat();
          switch (st.type) {
            case StatType::Unavailable: break;
            case StatType::Directory: read_state = ReadState::ReadingDirectory; break;
            default:
              dir_entry_end = dir_entry_start = 0;
              read_state = ReadState::ReadingFile; break;
          }
        }
        u16 success = 0, addr = peek2(dev + 0xac), len = peek2(dev + 0xaa);
        switch (read_state) {
          case ReadState::NotReading: break;
          case ReadState::ReadingFile: success = read(uxn.bounded_range_in_ram_mutable(addr, len)); break;
          case ReadState::ReadingDirectory: {
            auto out = uxn.bounded_range_in_ram_mutable(addr, len);
            u16 i = 0;
          next_entry:
            while (dir_entry_start < dir_entry_end && i < out.size)
              out[i++] = dir_entry[dir_entry_start++];
            Stat entry;
            if (list_dir(entry)) {
              write_dir_entry(entry);
              goto next_entry;
            }
            success = i;
          }
        }
        poke2(dev + 0xa2, success);
        return;
      }
      case 0xaf: {
        read_state = ReadState::NotReading;
        dir_entry_end = dir_entry_start = 0;
        u16 addr = peek2(dev + 0xae), len = peek2(dev + 0xaa);
        poke2(dev + 0xa2, write(uxn.bounded_range_in_ram_mutable(addr, len), dev[0xa7]));
        return;
      }
    }
  }
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

  void write_dir_entry(Stat stat) {
    dir_entry_start = 0;
    stat.write({ (u8*)dir_entry, 4 });
    dir_entry[4] = ' ';
    u16 i;
    for (i = 0; i < UXN_PATH_MAX && stat.name[i] != '\0'; i++) {
      dir_entry[5 + i] = stat.name[i];
    }
    dir_entry[5 + i] = '\n';
    dir_entry_end = 6 + i;
  }
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
  virtual bool init() {
    if (!base_screen->init()) return false;
    if (!base_file->init()) return false;
    if (boot_rom_filename) {
      size_t sz;
      boot_rom = base_file->load(boot_rom_filename, sz);
      if (!boot_rom) return false;
      boot_rom_size = static_cast<u32>(sz);
    }
    return Uxn::init();
  }

  virtual void reset(bool soft) {
    Uxn::reset(soft);
    base_screen->reset();
  }

  virtual void before_dei(u8 d) final {
    switch (d) {
      // System
      case 0x04: dev[d] = wst.ptr; return;
      case 0x05: dev[d] = rst.ptr; return;
      default:
      // Screen
      if (d >= 0x20 && d <= 0x2f) base_screen->before_dei(d);
      // Datetime
      else if (d >= 0xc0 && d <= 0xcf) dev[d] = base_datetime->datetime_byte(d & 0xf);
    }
  }

  virtual void after_deo(u8 d) final {
    switch (d) {
      // System
      case 0x03: {
        u16 addr = peek2(dev + 0x02);
        if (ram[addr] == 0x1 && addr <= 0x10000 - 10) {
          u8* cmd_addr = ram + addr + 1;
          u16 i, length = peek2(cmd_addr);
          u16 a_bank = peek2(cmd_addr + 2), a_addr = peek2(cmd_addr + 4);
          u16 b_bank = peek2(cmd_addr + 6), b_addr = peek2(cmd_addr + 8);
          u8 *src = bank(a_bank), *dst = bank(b_bank);
          for (i = 0; i < length; i++)
            dst[(b_addr + i) & 0xffff] = src[(a_addr + i) & 0xffff];
        }
        return;
      }
      case 0x04: wst.ptr = dev[0x04]; return;
      case 0x05: rst.ptr = dev[0x05]; return;
      case 0x09:
      case 0x0b:
      case 0x0d: base_screen->update_palette(); return;
      case 0x0e: on_system_debug(dev[0x0e]); return;
      default:
      // Screen
      if (d >= 0x20 && d <= 0x2f) base_screen->after_deo(d);
      // File
      else if (d >= 0xa0 && d <= 0xaf) base_file->after_deo(d);
    }
  }

protected:
  const char* boot_rom_filename;
  Console* base_console;
  Screen* base_screen;
  Input* base_input;
  Filesystem* base_file;
  Datetime* base_datetime;

  Varvara(Console* console, Screen* screen, Input* input, Filesystem* file, Datetime* time, const u8* rom, u32 rom_size)
  : Uxn(rom, rom_size),
    boot_rom_filename(nullptr),
    base_console(console),
    base_screen(screen),
    base_input(input),
    base_file(file),
    base_datetime(time) {}

  Varvara(Console* console, Screen* screen, Input* input, Filesystem* file, Datetime* time, const char* rom_filename)
  : Uxn(nullptr, 0),
    boot_rom_filename(rom_filename),
    base_console(console),
    base_screen(screen),
    base_input(input),
    base_file(file),
    base_datetime(time) {}

  virtual void on_system_debug(u8 b) {}
};

}
