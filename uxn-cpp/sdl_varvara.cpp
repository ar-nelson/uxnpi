#include "sdl_varvara.hpp"
#include <unistd.h>

#ifndef __plan9__
#define USED(x) (void)(x)
#endif

#define PAD 2
#define PAD2 4
#define TIMEOUT_MS 334

namespace uxn {

static u32 stdin_event = 0, audio0_event = 0;

int SdlVarvara::handle_events() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    /* Window */
    if (event.type == SDL_QUIT)
      return 0;
    else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_EXPOSED)
      screen.frame();
    //else if (event.type == SDL_DROPFILE) {
    //  if (!load_rom(event.drop.file)) abort();
    //  reset(false);
    //  SDL_free(event.drop.file);
    //}
    /* Mouse */
    else if (event.type == SDL_MOUSEMOTION)
      input.mouse_move(clamp(event.motion.x - PAD, 0, screen.width() - 1), clamp(event.motion.y - PAD, 0, screen.width() - 1));
    else if (event.type == SDL_MOUSEBUTTONUP)
      input.mouse_up((MouseButton)SDL_BUTTON(event.button.button));
    else if (event.type == SDL_MOUSEBUTTONDOWN)
      input.mouse_down((MouseButton)SDL_BUTTON(event.button.button));
    else if (event.type == SDL_MOUSEWHEEL)
      input.mouse_scroll(event.wheel.x, event.wheel.y);
    /* Controller */
    else if (event.type == SDL_TEXTINPUT) {
      char *c;
      for (c = event.text.text; *c; c++) {
        input.key_down(*c);
        input.key_up(*c);
      }
    } else if (event.type == SDL_KEYDOWN) {
      int ksym = event.key.keysym.sym;
      if (auto code = sdl_keycode(ksym, SDL_GetModState()))
        input.key_down(code);
      else if (event.key.keysym.sym == SDLK_F1)
        screen.toggle_zoom();
      else if (event.key.keysym.sym == SDLK_F2)
        set_debugger(dev[0x0e]);
      //else if (event.key.keysym.sym == SDLK_F3)
      //  capture_screen();
      else if (event.key.keysym.sym == SDLK_F4)
        reset(false);
      else if (event.key.keysym.sym == SDLK_F5)
        reset(true);
      else if (event.key.keysym.sym == SDLK_F11)
        screen.toggle_fullscreen();
      else if (event.key.keysym.sym == SDLK_F12)
        screen.toggle_borderless();
      if (SDL_PeepEvents(&event, 1, SDL_PEEKEVENT, SDL_KEYUP, SDL_KEYUP) == 1 && ksym == event.key.keysym.sym)
        return 1;
    } else if (event.type == SDL_KEYUP) {
      if (auto code = sdl_keycode(event.key.keysym.sym, SDL_GetModState()))
        input.key_up(code);
    }
    else if (event.type == SDL_JOYAXISMOTION) {
      u8 vec = get_vector_joystick(&event);
      if (!vec)
        input.button_up((Button)((3 << (!event.jaxis.axis * 2)) << 4));
      else
        input.button_down((Button)((1 << ((vec + !event.jaxis.axis * 2) - 1)) << 4));
    } else if (event.type == SDL_JOYBUTTONDOWN)
      input.button_down(get_button_joystick(&event));
    else if (event.type == SDL_JOYBUTTONUP)
      input.button_up(get_button_joystick(&event));
    else if (event.type == SDL_JOYHATMOTION) {
      /* NOTE: Assuming there is only one joyhat in the controller */
      switch(event.jhat.value) {
        case SDL_HAT_UP: input.button_down(Button::Up); break;
        case SDL_HAT_DOWN: input.button_down(Button::Down); break;
        case SDL_HAT_LEFT: input.button_down(Button::Left); break;
        case SDL_HAT_RIGHT: input.button_down(Button::Right); break;
        case SDL_HAT_LEFTDOWN: input.button_down(Button::Left | Button::Down); break;
        case SDL_HAT_LEFTUP: input.button_down(Button::Left | Button::Up); break;
        case SDL_HAT_RIGHTDOWN: input.button_down(Button::Right | Button::Down); break;
        case SDL_HAT_RIGHTUP: input.button_down(Button::Right | Button::Up); break;
        case SDL_HAT_CENTERED: input.button_down(Button::Up | Button::Down | Button::Left | Button::Right); break;
      }
    }
    /* Console */
    else if (event.type == stdin_event)
      console.read_byte(event.cbutton.button, ConsoleType::Stdin);
  }
  return 1;
}

/* Handlers */

void SdlVarvara::audio_finished_handler(int instance) {
  SDL_Event event;
  event.type = audio0_event + instance;
  SDL_PushEvent(&event);
}

static int stdin_handler(void *p) {
  SDL_Event event;
  USED(p);
  event.type = stdin_event;
  while (read(0, &event.cbutton.button, 1) > 0) {
    while (SDL_PushEvent(&event) < 0)
      SDL_Delay(25); /* slow down - the queue is most likely full */
  }
  return 0;
}

static inline void set_window_size(SDL_Window *window, SDL_Renderer* renderer, int w, int h) {
  SDL_Point win_old;
  SDL_GetWindowSize(window, &win_old.x, &win_old.y);
  if (w == win_old.x && h == win_old.y) return;
  SDL_RenderClear(renderer);
  SDL_SetWindowSize(window, w, h);
}

void SdlScreen::set_zoom(u8 z, bool win) {
  if (z < 1) return;
  if (win) {
    set_window_size(emu_window, emu_renderer, (w + PAD2) * z, (h + PAD2) * z);
    repaint();
  }
  zoom = z;
}

void SdlScreen::set_fullscreen(bool value, bool win) {
  u32 flags = 0; /* windowed mode; SDL2 has no constant for this */
  fullscreen = value;
  if (fullscreen) flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
  if (win) SDL_SetWindowFullscreen(emu_window, flags);
}

void SdlScreen::set_borderless(bool value) {
  if (fullscreen) return;
  borderless = value;
  SDL_SetWindowBordered(emu_window, (SDL_bool)!value);
}

void SdlScreen::on_resize() {
  if (emu_texture != nullptr)
    SDL_DestroyTexture(emu_texture);
  SDL_RenderSetLogicalSize(emu_renderer, w + PAD2, h + PAD2);
  emu_texture = SDL_CreateTexture(emu_renderer, SDL_PIXELFORMAT_BGR888, SDL_TEXTUREACCESS_STATIC, w, h);
  if (emu_texture == nullptr || SDL_SetTextureBlendMode(emu_texture, SDL_BLENDMODE_NONE)) {
    error_message("SDL_SetTextureBlendMode", SDL_GetError());
    return;
  }
  emu_viewport.x = PAD;
  emu_viewport.y = PAD;
  emu_viewport.w = w;
  emu_viewport.h = h;
  set_window_size(emu_window, emu_renderer, (w + PAD2) * zoom, (h + PAD2) * zoom);
}

bool SdlScreen::init() {
  u32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
  if (fullscreen)
    window_flags = window_flags | SDL_WINDOW_FULLSCREEN_DESKTOP;
  emu_window = SDL_CreateWindow("UXN",
    SDL_WINDOWPOS_UNDEFINED,
    SDL_WINDOWPOS_UNDEFINED,
    w * zoom,
    h * zoom,
    window_flags
  );
  if (emu_window == nullptr) {
    error_message("sdl_window", SDL_GetError());
    return false;
  }
  emu_renderer = SDL_CreateRenderer(emu_window, -1, SDL_RENDERER_ACCELERATED);
  if (emu_renderer == nullptr) {
    error_message("sdl_renderer", SDL_GetError());
    return false;
  }
  SDL_SetRenderDrawColor(emu_renderer, 0x00, 0x00, 0x00, 0xff);
  on_resize();
  return true;
}

void SdlVarvara::set_debugger(u8 value) {
  dev[0x0e] = value;
}

bool SdlVarvara::init() {
  //SDL_AudioSpec as;
  //SDL_zero(as);
  //as.freq = SAMPLE_FREQUENCY;
  //as.format = AUDIO_S16SYS;
  //as.channels = 2;
  //as.callback = audio_handler;
  //as.samples = AUDIO_BUFSIZE;
  //as.userdata = this;
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) {
    error_message("sdl", SDL_GetError());
    return false;
  }
  //audio_id = SDL_OpenAudioDevice(nullptr, 0, &as, nullptr, 0);
  //if (!audio_id) {
  //  out_system_error("sdl_audio", SDL_GetError());
  //  return false;
  //}
  if (SDL_NumJoysticks() > 0 && SDL_JoystickOpen(0) == nullptr) {
    error_message("sdl_joystick", SDL_GetError());
    return false;
  }
  if (!stdin_event) stdin_event = SDL_RegisterEvents(1);
  //if (!audio0_event) audio0_event = SDL_RegisterEvents(POLYPHONY);
  SDL_DetachThread(stdin_thread = SDL_CreateThread(stdin_handler, "stdin", nullptr));
  SDL_StartTextInput();
  SDL_ShowCursor(SDL_DISABLE);
  //SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
  ms_interval = SDL_GetPerformanceFrequency() / 1000;
  deadline_interval = ms_interval * TIMEOUT_MS;
  exec_deadline = SDL_GetPerformanceCounter() + deadline_interval;
  //SDL_PauseAudioDevice(audio_id, 1);
  return Varvara::init();
}

u8 SdlVarvara::run() {
  if (!initialized) return 0;
  u64 next_refresh = 0;
  u64 frame_interval = SDL_GetPerformanceFrequency() / 60;

  /* game loop */
  eval(PAGE_PROGRAM);
  while (!exit_state) {
    u64 now = SDL_GetPerformanceCounter();
    /* .System/halt */
    if (dev[0x0f]) {
      error_message("Run", "Ended.");
      break;
    }
    exec_deadline = now + deadline_interval;
    if (!handle_events()) return false;
    bool should_wait = true;
    if (now >= next_refresh) {
      now = SDL_GetPerformanceCounter();
      next_refresh = now + frame_interval;
      should_wait = screen.frame();
    }
    if (should_wait) {
      u64 delay_ms = (next_refresh - now) / ms_interval;
      if (delay_ms > 0) SDL_Delay(delay_ms);
    } else
      SDL_WaitEvent(nullptr);
  }

  return exit_state;
}

SdlVarvara::~SdlVarvara() {
  /* cleanup */
  //SDL_CloseAudioDevice(audio_id);
#ifdef _WIN32
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
  TerminateThread((HANDLE)SDL_GetThreadID(stdin_thread), 0);
#elif !defined(__APPLE__)
  close(0); /* make stdin thread exit */
#endif
}

}

int main(int argc, char **argv) {
  int i = 1;
  u8 zoom = 0;
  bool fullscreen = false;
  /* flags */
  if (argc > 1 && argv[i][0] == '-') {
    if (!strcmp(argv[i], "-v")) {
      fprintf(stderr, "UXM C++ SDL DEMO\n");
      return 0;
    } else if (!strcmp(argv[i], "-2x")) {
      zoom = 2;
    } else if (!strcmp(argv[i], "-3x")) {
      zoom = 3;
    } else if (strcmp(argv[i], "-f") == 0) {
      fullscreen = true;
    }
    i++;
  }
  const char* rom_name = i == argc ? "boot.rom" : argv[i++];
  char cwd[uxn::UXN_PATH_MAX / 2];
  getcwd(cwd, sizeof(cwd));
  uxn::SdlVarvara uxn(640, 480, cwd, rom_name);
  if (!uxn.init()) return 1;
  return uxn.run();
}
