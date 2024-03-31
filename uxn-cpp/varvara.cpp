#include "varvara.hpp"

namespace uxn {

////////////////////////////////////////////////////////////
// CONSOLE

bool Console::read_args(int argc, char **argv) {
  for (int i = 0; i < argc; i++) {
    if (i) if (!read_byte(' ', ConsoleType::ArgumentSpacer)) return false;
    for (size_t c = 0; argv[i][c] != '\0'; c++) {
      if (!read_byte(argv[i][c], ConsoleType::Argument)) return false;
    }
  }
  return read_byte(0, ConsoleType::ArgumentEnd);
}

////////////////////////////////////////////////////////////
// SCREEN

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

void Screen::before_dei(u8 d) {
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

void Screen::after_deo(u8 d) {
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

void Screen::try_resize(u16 width, u16 height) {
  if (width < 0x8 || height < 0x8 || width >= 0x800 || height >= 0x800)
    return;
  if (w == width && h == height)
    return;
  delete[] bg;
  delete[] fg;
  bg = new u8[width * height], fg = new u8[width * height];
  w = width, h = height;
  fill(bg, 0), fill(fg, 0);
  change(0, 0, width, height);
}

void Screen::change(u16 x1, u16 y1, u16 x2, u16 y2) {
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

void Screen::fill(u8 *layer, u8 color) {
  size_t i, length = w * h;
  for (i = 0; i < length; i++)
    layer[i] = color;
  dirty = true;
}

void Screen::rect(u8 *layer, u16 x1, u16 y1, u16 x2, u16 y2, u8 color) {
  size_t row, x, y;
  for (y = y1; y < y2 && y < h; y++)
    for (x = x1, row = y * w; x < x2 && x < w; x++)
      layer[x + row] = color;
  dirty = true;
}

void Screen::sprite_2bpp(u8 *layer, const u8 *addr, u16 x1, u16 y1, u8 color, int fx, int fy) {
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

void Screen::sprite_1bpp(u8 *layer, const u8 *addr, u16 x1, u16 y1, u8 color, int fx, int fy) {
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

void Screen::draw_byte(u8 b, u16 x, u16 y, u8 color) {
  sprite_1bpp(fg, &icons[(b >> 4) << 3], x, y, color, 1, 1);
  sprite_1bpp(fg, &icons[(b & 0xf) << 3], x + 8, y, color, 1, 1);
  change(x, y, x + 0x10, y + 0x8);
}

void Screen::debugger() {
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

void Screen::redraw() {
  constexpr u8 palette_map[16] = { 0, 1, 2, 3, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3 };
  u16 x1 = screen_x1, y1 = screen_y1;
  u16 x2 = screen_x2 > w ? w : screen_x2, y2 = screen_y2 > h ? h : screen_y2;
  screen_x1 = screen_y1 = 0xffff;
  screen_x2 = screen_y2 = 0;
  if (uxn.dev[0x0e])
    debugger();
  for (u16 y = y1; y < y2; y++) {
    for (u16 x = x1; x < x2; x++) {
      const size_t i = y * w + x;
      on_pixel(x, y, palette_map[fg[i] << 2 | bg[i]]);
    }
  }
}

////////////////////////////////////////////////////////////
// AUDIO

static constexpr float
  SOUND_TIMER = AUDIO_BUFSIZE / SAMPLE_FREQUENCY * 1000.0f;

#define XFADE_SAMPLES 100
#define INTERPOL_METHOD 1

static constexpr float tuning[109] = {
  0.00058853f, 0.00062352f, 0.00066060f, 0.00069988f, 0.00074150f,
  0.00078559f, 0.00083230f, 0.00088179f, 0.00093423f, 0.00098978f,
  0.00104863f, 0.00111099f, 0.00117705f, 0.00124704f, 0.00132120f,
  0.00139976f, 0.00148299f, 0.00157118f, 0.00166460f, 0.00176359f,
  0.00186845f, 0.00197956f, 0.00209727f, 0.00222198f, 0.00235410f,
  0.00249409f, 0.00264239f, 0.00279952f, 0.00296599f, 0.00314235f,
  0.00332921f, 0.00352717f, 0.00373691f, 0.00395912f, 0.00419454f,
  0.00444396f, 0.00470821f, 0.00498817f, 0.00528479f, 0.00559904f,
  0.00593197f, 0.00628471f, 0.00665841f, 0.00705434f, 0.00747382f,
  0.00791823f, 0.00838908f, 0.00888792f, 0.00941642f, 0.00997635f,
  0.01056957f, 0.01119807f, 0.01186395f, 0.01256941f, 0.01331683f,
  0.01410869f, 0.01494763f, 0.01583647f, 0.01677815f, 0.01777583f,
  0.01883284f, 0.01995270f, 0.02113915f, 0.02239615f, 0.02372789f,
  0.02513882f, 0.02663366f, 0.02821738f, 0.02989527f, 0.03167293f,
  0.03355631f, 0.03555167f, 0.03766568f, 0.03990540f, 0.04227830f,
  0.04479229f, 0.04745578f, 0.05027765f, 0.05326731f, 0.05643475f,
  0.05979054f, 0.06334587f, 0.06711261f, 0.07110333f, 0.07533136f,
  0.07981079f, 0.08455659f, 0.08958459f, 0.09491156f, 0.10055530f,
  0.10653463f, 0.11286951f, 0.11958108f, 0.12669174f, 0.13422522f,
  0.14220667f, 0.15066272f, 0.15962159f, 0.16911318f, 0.17916918f,
  0.18982313f, 0.20111060f, 0.21306926f, 0.22573902f, 0.23916216f,
  0.25338348f, 0.26845044f, 0.28441334f, 0.30132544f,
};

void Envelope::on() {
  stage = EnvStage::Attack;
  vol = 0.0f;
  if (a > 0) {
    a = (SOUND_TIMER / AUDIO_BUFSIZE) / a;
  } else if (stage == EnvStage::Attack) {
    stage = EnvStage::Decay;
    vol = 1.0f;
  }
  if (d < 10.0f) d = 10.0f;
  d = (SOUND_TIMER / AUDIO_BUFSIZE) / d;
  if (r < 10.0f) r = 10.0f;
  r = (SOUND_TIMER / AUDIO_BUFSIZE) / r;
}

void Envelope::off() {
  stage = EnvStage::Release;
}

void AudioChannel::note_on(float dur, u8 *data, u16 len, u8 vol, u8 attack, u8 decay, u8 sustain, u8 release, u8 pitch, bool loop) {
  duration = dur;
  vol_l = (vol >> 4) / 15.0f;
  vol_r = (vol & 0xf) / 15.0f;

  Sample sample{0};
  sample.data = data;
  sample.len = len;
  sample.pos = 0;
  sample.env.a = attack * 64.0f;
  sample.env.d = decay * 64.0f;
  sample.env.s = sustain / 16.0f;
  sample.env.r = release * 64.0f;
  if (loop) sample.loop = len;
  else sample.loop = 0;
  sample.env.on();
  float sample_rate = 44100 / 261.60;
  if(len <= 256) {
    sample_rate = len;
  }
  const float *inc = &tuning[pitch - 20];
  sample.inc = *(inc)*sample_rate;

  next_sample = sample;
  xfade = true;
}

void AudioChannel::note_off(float dur) {
  duration = dur;
  sample.env.off();
}

void Envelope::advance() {
  switch (stage) {
    case EnvStage::Attack:
      vol += a;
      if (vol >= 1.0f) {
        stage = EnvStage::Decay;
        vol = 1.0f;
      }
      break;
    case EnvStage::Decay:
      vol -= d;
      if (vol <= s || d <= 0) {
        stage = EnvStage::Sustain;
        vol = s;
      }
      break;
    case EnvStage::Sustain:
      vol = s;
      break;
    case EnvStage::Release:
      if (vol <= 0 || r <= 0) {
        vol = 0;
      } else {
        vol -= r;
      }
      break;
  }
}

static inline float interpolate_sample(u8 *data, u16 len, float pos) {
#if INTERPOL_METHOD == 0
    return data[(int)pos];

#elif INTERPOL_METHOD == 1
    float x = pos;
    int x0 = (int)x;
    int x1 = (x0 + 1);
    float y0 = data[x0];
    float y1 = data[x1 % len];
    x = x - x0;
    float y = y0 + x * (y1 - y0);
    return y;

#elif INTERPOL_METHOD == 2
    float x = pos;
    int x0 = x - 1;
    int x1 = x;
    int x2 = x + 1;
    int x3 = x + 2;
    float y0 = data[x0 % len];
    float y1 = data[x1];
    float y2 = data[x2 % len];
    float y3 = data[x3 % len];
    x = x - x1;
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
    float c3 = 1.5f * (y1 - y2) + 0.5f * (y3 - y0);
    return ((c3 * x + c2) * x + c1) * x + c0;
#endif
}

s16 Sample::next() {
  if (pos >= len) {
    if (loop == 0) {
      data = 0;
      return 0;
    }
    while (pos >= len) {
      pos -= loop;
    }
  }

  float val = interpolate_sample(data, len, pos);
  val *= env.vol;
  s8 next = (s8)0x80 ^ (u8)val;

  pos += inc;
  env.advance();
  return next;
}

void Audio::write(u8* out_stream, size_t len) {
  s16* stream = reinterpret_cast<s16*>(out_stream);
  for (size_t i = 0; i < len / 2; i++) stream[i] = 0;

  for (u8 n = 0; n < POLYPHONY; n++) {
    u8 device = (3 + n) << 4;
    u8* addr = &uxn.dev[device];
    if (channel[n].duration <= 0) {
      uxn.call_vec(device);
    }
    channel[n].duration -= SOUND_TIMER;

    unsigned x = 0;
    if (channel[n].xfade) {
      float delta = 1.0f / (XFADE_SAMPLES * 2);
      while (x < XFADE_SAMPLES * 2) {
        float alpha = x * delta;
        float beta = 1.0f - alpha;
        s16 next_a = channel[n].next_sample.next();
        s16 next_b = 0;
        if (channel[n].sample.data != 0) {
          next_b = channel[n].sample.next();
        }
        s16 next = alpha * next_a + beta * next_b;
        stream[x++] += next * channel[n].vol_l;
        stream[x++] += next * channel[n].vol_r;
      }
      channel[n].sample = channel[n].next_sample;
      channel[n].xfade = false;
    }
    Sample& sample = channel[n].sample;
    while (x < len / 2) {
      if (sample.data == 0) break;
      s16 next = sample.next();
      stream[x++] += next * channel[n].vol_l;
      stream[x++] += next * channel[n].vol_r;
    }
  }
  for (size_t i = 0; i < len / 2; i++) {
    stream[i] <<= 6;
  }
}

static inline float calc_duration(u16 len, u8 pitch) {
  float scale = tuning[pitch - 20] / tuning[0x3c - 20];
  return len / (scale * 44.1f);
}

void Audio::start(u8 instance) {
  u8* d = &uxn.dev[(3 + instance) << 4];
  u16 dur = peek2(d + 0x5);
  u8 off = d[0xf] == 0x00;
  u16 len = peek2(d + 0xa);
  u8 pitch = d[0xf] & 0x7f;
  if (pitch < 20) pitch = 20;
  float duration = dur > 0 ? dur : calc_duration(len, pitch);

  if (!off) {
    u16 addr = peek2(d + 0xc);
    u8* data = &uxn.ram[addr];
    u8 volume = d[0xe];
    bool loop = !(d[0xf] & 0x80);
    u16 adsr = peek2(d + 0x8);
    u8 attack = (adsr >> 12) & 0xF;
    u8 decay = (adsr >> 8) & 0xF;
    u8 sustain = (adsr >> 4) & 0xF;
    u8 release = (adsr >> 0) & 0xF;
    channel[instance].note_on(duration, data, len, volume, attack, decay, sustain, release, pitch, loop);
  } else {
    channel[instance].note_off(duration);
  }
}

void Audio::before_dei(u8 d) {
  u8& b = uxn.dev[d];
  switch (d) {
    case 0x32: b = get_position(0) >> 8; return;
    case 0x33: b = get_position(0); return;
    case 0x34: b = get_vu(0); return;
    case 0x42: b = get_position(1) >> 8; return;
    case 0x43: b = get_position(1); return;
    case 0x44: b = get_vu(1); return;
    case 0x52: b = get_position(2) >> 8; return;
    case 0x53: b = get_position(2); return;
    case 0x54: b = get_vu(2); return;
    case 0x62: b = get_position(3) >> 8; return;
    case 0x63: b = get_position(3); return;
    case 0x64: b = get_vu(3); return;
  }
}

void Audio::after_deo(u8 d) {
  switch (d) {
    case 0x3f: start(0); return;
    case 0x4f: start(1); return;
    case 0x5f: start(2); return;
    case 0x6f: start(3); return;
  }
}

////////////////////////////////////////////////////////////
// FILESYSTEM

void Stat::write(MutableSlice out) {
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

void Filesystem::after_deo(u8 d) {
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

void Filesystem::write_dir_entry(Stat stat) {
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


bool Varvara::init() {
  if (!base_screen->init()) return false;
  if (!base_audio->init()) return false;
  if (!base_file->init()) return false;
  if (boot_rom_filename) {
    size_t sz;
    boot_rom = base_file->load(boot_rom_filename, sz);
    if (!boot_rom) return false;
    boot_rom_size = static_cast<u32>(sz);
  }
  return Uxn::init();
}

void Varvara::reset(bool soft) {
  Uxn::reset(soft);
  base_screen->reset();
}

void Varvara::before_dei(u8 d) {
  switch (d) {
    // System
    case 0x04: dev[d] = wst.ptr; return;
    case 0x05: dev[d] = rst.ptr; return;
    default:
    // Screen
    if (d >= 0x20 && d <= 0x2f) base_screen->before_dei(d);
    // Audio
    else if (d >= 0x30 && d <= 0x6f) base_audio->before_dei(d);
    // Datetime
    else if (d >= 0xc0 && d <= 0xcf) dev[d] = base_datetime->datetime_byte(d & 0xf);
  }
}

void Varvara::after_deo(u8 d) {
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
    // Audio
    else if (d >= 0x30 && d <= 0x6f) base_audio->after_deo(d);
    // File
    else if (d >= 0xa0 && d <= 0xaf) base_file->after_deo(d);
  }
}
}
