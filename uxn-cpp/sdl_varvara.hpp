#pragma once
#include "varvara.hpp"
#include "stdlib_console.hpp"
#include "stdlib_filesystem.hpp"
#include "posix_datetime.hpp"
#include <SDL2/SDL.h>

namespace uxn {

static constexpr u8 sdl_keycode(int sym, SDL_Keymod mods = KMOD_NONE) {
  if (sym < 0x20 || sym == SDLK_DELETE)
    return sym;
  if (mods & KMOD_CTRL) {
    if (sym < SDLK_a)
      return sym;
    else if (sym <= SDLK_z)
      return sym - (mods & KMOD_SHIFT) * 0x20;
  }
  return 0x00;
}

static inline void error_message(const char* ctx, const char* msg) {
  std::cerr << ctx << ": " << msg << std::endl;
}

class SdlScreen : public PixelScreen<SDL_Color> {
private:
  SDL_Window* emu_window = nullptr;
  SDL_Texture* emu_texture = nullptr;
  SDL_Renderer* emu_renderer = nullptr;
  SDL_Rect emu_viewport;
  u8 zoom;
  bool fullscreen, borderless;

public:
  SdlScreen(Uxn& uxn, u16 w, u16 h, u8 zoom = 1, bool fullscreen = false, bool borderless = false) :
    PixelScreen(uxn, w, h),
    zoom(zoom),
    fullscreen(fullscreen),
    borderless(borderless) {}
  virtual ~SdlScreen() {}

  virtual bool init();
  virtual SDL_Color color_from_12bit(u8 r, u8 g, u8 b, u8 ix) const final {
    return { .r = (u8)(r | (r << 4)), .g = (u8)(g | (g << 4)), .b = (u8)(b | (b << 4)) };
  };
  virtual void on_paint() {
    if (!emu_renderer) return;
    if (SDL_UpdateTexture(emu_texture, NULL, pixels, w * sizeof(SDL_Color)) != 0)
      error_message("SDL_UpdateTexture", SDL_GetError());
    SDL_RenderClear(emu_renderer);
    SDL_RenderCopy(emu_renderer, emu_texture, NULL, &emu_viewport);
    SDL_RenderPresent(emu_renderer);
  }
  virtual void on_resize();

  void set_zoom(u8 z, bool win);
  void set_fullscreen(bool value, bool win);
  void set_borderless(bool value);

  void toggle_fullscreen() { set_fullscreen(!fullscreen, 1); }
  void toggle_borderless() { set_borderless(!borderless); }
  void toggle_zoom() { set_zoom(zoom == 3 ? 1 : zoom + 1, 1); }
};

class SdlAudio : public Audio {
private:
  static void audio_handler(void* self, u8* out, int len) {
    auto* audio = reinterpret_cast<SdlAudio*>(self);
    audio->write(out, len);
  }
  SDL_AudioDeviceID audio_id;
public:
  SdlAudio(Uxn& uxn) : Audio(uxn), audio_id(0) {}
  virtual ~SdlAudio() {
    if (audio_id) SDL_CloseAudioDevice(audio_id);
  }

  bool init() final {
    if (audio_id) return true;
    SDL_AudioSpec as;
    SDL_zero(as);
    as.freq = SAMPLE_FREQUENCY;
    as.format = AUDIO_S16SYS;
    as.channels = 2;
    as.callback = audio_handler;
    as.samples = AUDIO_BUFSIZE;
    as.userdata = this;
    audio_id = SDL_OpenAudioDevice(nullptr, 0, &as, nullptr, 0);
    if (!audio_id) {
      error_message("sdl_audio", SDL_GetError());
      return true; // don't fail, having no audio is not a fatal error
    }
    SDL_PauseAudioDevice(audio_id, 1);
    return true;
  }

  void start(u8 instance) final {
    if (!audio_id) return;
    SDL_LockAudioDevice(audio_id);
    Audio::start(instance);
    SDL_UnlockAudioDevice(audio_id);
    SDL_PauseAudioDevice(audio_id, 0);
  }
};

class SdlVarvara : public Varvara {
protected:
  StdlibConsole console;
  SdlScreen screen;
  SdlAudio audio;
  KeyMapInput input;
  StdlibFilesystem file;
  PosixDatetime datetime;

  //SDL_AudioDeviceID audio_id;
  SDL_Thread* stdin_thread = nullptr;

  u8 exit_state = 0;
  u64 exec_deadline, deadline_interval, ms_interval;

  static Button get_button_joystick(SDL_Event* event) {
    return static_cast<Button>(0x01 << (event->jbutton.button & 0x3));
  }
  static u8 get_vector_joystick(SDL_Event* event) {
    if (event->jaxis.value < -3200) return 1;
    if (event->jaxis.value > 3200) return 2;
    return 0;
  }
  int handle_events();

  void audio_finished_handler(int instance);
  void set_debugger(u8 value);

public:
  static constexpr KeyMap default_key_map {
    .a = sdl_keycode(SDLK_LCTRL),
    .b = sdl_keycode(SDLK_LALT),
    .select = sdl_keycode(SDLK_LSHIFT),
    .start = sdl_keycode(SDLK_HOME),
    .up = sdl_keycode(SDLK_UP),
    .down = sdl_keycode(SDLK_DOWN),
    .left = sdl_keycode(SDLK_LEFT),
    .right = sdl_keycode(SDLK_RIGHT)
  };

  SdlVarvara(u16 w, u16 h, const char* root_dir, const u8* rom, u32 rom_size)
  : Varvara(&console, &screen, &audio, &input, &file, &datetime, rom, rom_size),
    console(*this),
    screen(*this, w, h),
    audio(*this),
    input(*this, default_key_map),
    file(*this, root_dir) {}

  SdlVarvara(u16 w, u16 h, const char* root_dir, const char* rom_filename = "boot.rom")
  : Varvara(&console, &screen, &audio, &input, &file, &datetime, rom_filename),
    console(*this),
    screen(*this, w, h),
    audio(*this),
    input(*this, default_key_map),
    file(*this, root_dir) {}

  virtual ~SdlVarvara();

  virtual bool init();
  u8 run();
};

}
