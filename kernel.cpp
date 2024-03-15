#include "kernel.hpp"
#include <circle/string.h>
#include <circle/util.h>
#include <assert.h>

extern "C" {
#include "uxn-fast.c"
#include "rom.c"
}

#define DEVICE_INDEX  1    // "upad1"

RaspiKernel* RaspiKernel::instance = 0;
UxnKernel* UxnKernel::instance = 0;

static const char FromKernel[] = "kernel";
static const char FromUxn[] = "uxn";

RaspiKernel::RaspiKernel() :
  screen(640, 480), timer(&interrupt), logger(options.GetLogLevel(), &timer),
  usb_hci(&interrupt, &timer, TRUE /* TRUE = enable PnP */), game_pad(0)
{
  instance = this;
  act_led.Blink(5);  // show we are alive
}

RaspiKernel::~RaspiKernel() {
  instance = 0;
}

bool RaspiKernel::initialize() {
  bool OK = TRUE;

  if (OK) OK = screen.Initialize();
  if (OK) OK = serial.Initialize(115200);
  if (OK) {
    CDevice* target = device_name_service.GetDevice(options.GetLogDevice(), FALSE);
    if (target == 0) {
      target = &screen;
    }

    OK = logger.Initialize(target);
  }
  if (OK) OK = interrupt.Initialize();
  if (OK) OK = timer.Initialize();
  if (OK) OK = usb_hci.Initialize();

  return OK;
}

UxnKernel::UxnKernel(
  RaspiKernel& kernel,
  const u8* rom,
  u16 rom_size
) : ppu(&kernel.screen), timer(kernel.timer), logger(kernel.logger) {
  instance = this;
  logger.Write(FromUxn, LogNotice, "Initializing UXN.");

  // Clear UXN memory.
  memset(&u, 0, sizeof(Uxn));

  // Copy rom to VM.
  memcpy(u.ram.dat + PAGE_PROGRAM, rom, rom_size);

  // Prepare devices.
  uxn_port(&u, 0x0, "system", system_talk);
  uxn_port(&u, 0x1, "console", console_talk);
  devscreen = uxn_port(&u, 0x2, "screen", screen_talk);
  devaudio = uxn_port(&u, 0x3, "audio0", audio_talk);
  uxn_port(&u, 0x4, "audio1", audio_talk);
  uxn_port(&u, 0x5, "audio2", audio_talk);
  uxn_port(&u, 0x6, "audio3", audio_talk);
  uxn_port(&u, 0x7, "---", nil_talk);
  devctrl = uxn_port(&u, 0x8, "controller", nil_talk);
  devmouse = uxn_port(&u, 0x9, "mouse", nil_talk);
  uxn_port(&u, 0xa, "file", file_talk);
  uxn_port(&u, 0xb, "datetime", datetime_talk);
  uxn_port(&u, 0xc, "---", nil_talk);
  uxn_port(&u, 0xd, "---", nil_talk);
  uxn_port(&u, 0xe, "---", nil_talk);
  uxn_port(&u, 0xf, "---", nil_talk);
  mempoke16(devscreen->dat, 2, ppu.width);
  mempoke16(devscreen->dat, 4, ppu.height);
}

UxnKernel::~UxnKernel() {
  instance = 0;
}

bool UxnKernel::run() {
  uxn_eval(&u, 0x0100);
  u64 current_ticks = timer.GetClockTicks64();
  while (true) {
    // Echo input to standard output.
    uxn_eval(&u, mempeek16(devscreen->dat, 0));

    // Blit ppu.pixels to the framebuffer.
    ppu.blit();

    // Sync at 60 Hz.
    if ((timer.GetClockTicks64() - current_ticks) < 16666) {
      timer.usDelay(16666 - (timer.GetClockTicks64() - current_ticks));
    }
    current_ticks = timer.GetClockTicks64();
  }
  return TRUE;
}

void UxnKernel::nil_talk(Device* d, u8 b0, u8 w) {
  (void)d;
  (void)b0;
  (void)w;
}

void UxnKernel::console_talk(Device* d, u8 b0, u8 w) {
  assert(instance != 0);
  char stmp[2];
  if(!w) {
    return;
  }
  switch(b0) {
    case 0x8: stmp[0] = d->dat[0x8]; stmp[1] = 0; instance->logger.Write(FromUxn, LogNotice, stmp); break;
    // TODO: implement printf for the uart to be able to format
    // numbers.
    // case 0x9: txt_printf("0x%02x", d->dat[0x9]); break;
    // case 0xb: txt_printf("0x%04x", mempeek16(d->dat, 0xa)); break;
    // case 0xd: txt_printf("%s", &d->mem[mempeek16(d->dat, 0xc)]); break;
  }
}

void UxnKernel::system_talk(Device* d, u8 b0, u8 w) {
  assert(instance != 0);
  instance->logger.Write(FromUxn, LogNotice, "system_talk");
  if(!w) { /* read */
    switch(b0) {
    case 0x2: d->dat[0x2] = d->u->wst.ptr; break;
    case 0x3: d->dat[0x3] = d->u->rst.ptr; break;
    }
  } else { /* write */
    switch(b0) {
    case 0x2: d->u->wst.ptr = d->dat[0x2]; break;
    case 0x3: d->u->rst.ptr = d->dat[0x3]; break;
    case 0xf: d->u->ram.ptr = 0x0000; break;
    }
    if(b0 > 0x7 && b0 < 0xe) {
      instance->ppu.set_color_byte(b0 - 0x8, d->dat[b0]);
    }
  }
  (void)b0;
}

void UxnKernel::screen_talk(Device *d, u8 b0, u8 w) {
  assert(instance != 0);
  //instance->logger.Write(FromUxn, LogNotice, "screen_talk");
  if(w && b0 == 0xe) {
    u16 x = mempeek16(d->dat, 0x8);
    u16 y = mempeek16(d->dat, 0xa);
    u8 layer = d->dat[0xe] >> 4 & 0x1;
    instance->ppu.pixel(layer, x, y, d->dat[0xe] & 0x3);
  } else if(w && b0 == 0xf) {
    u16 x = mempeek16(d->dat, 0x8);
    u16 y = mempeek16(d->dat, 0xa);
    u8 layer = d->dat[0xf] >> 0x6 & 0x1;
    u8 *addr = &d->mem[mempeek16(d->dat, 0xc)];
    if(d->dat[0xf] >> 0x7 & 0x1)
      instance->ppu.sprite_2bpp(layer, x, y, addr, d->dat[0xf] & 0xf, d->dat[0xf] >> 0x4 & 0x1, d->dat[0xf] >> 0x5 & 0x1);
    else
      instance->ppu.sprite_1bpp(layer, x, y, addr, d->dat[0xf] & 0xf, d->dat[0xf] >> 0x4 & 0x1, d->dat[0xf] >> 0x5 & 0x1);
  }
}

void UxnKernel::audio_talk(Device *d, u8 b0, u8 w) {
  // TODO: Implement...
  (void)d;
  (void)b0;
  (void)w;
}

void UxnKernel::datetime_talk(Device *d, u8 b0, u8 w) {
  // TODO: Implement...
  (void)d;
  (void)b0;
  (void)w;
}

void UxnKernel::file_talk(Device *d, u8 b0, u8 w) {
  // TODO: Implement...
  (void)d;
  (void)b0;
  (void)w;
}

ShutdownMode RaspiKernel::run() {
  logger.Write(FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

  // Try to get a gamepad ONCE.
  // TODO: This probably doesn't need PnP.

  /*
  screen.Rotor(0, 0);
  timer.MsDelay(20);

  // This must be called from TASK_LEVEL to update the tree of connected USB devices.
  bool updated = usb_hci.UpdatePlugAndPlay();

  if (game_pad == 0) {
    if (!updated) goto no_gamepad;

    game_pad_known = FALSE;

    // get pointer to gamepad device and check if it is supported
    game_pad = (CUSBGamePadDevice*)
      device_name_service.GetDevice("upad", DEVICE_INDEX, FALSE);
    if (game_pad == 0) goto no_gamepad;

    game_pad->RegisterRemovedHandler(GamePadRemovedHandler);

    if (!(game_pad->GetProperties() & GamePadPropertyIsKnown)) {
      logger.Write(FromKernel, LogWarning, "Gamepad mapping is not known");
      goto no_gamepad;
    }

    game_pad_known = TRUE;

    // get initial state from gamepad and register status handler
    const TGamePadState* state = game_pad->GetInitialState();
    assert(state != 0);
    GamePadStatusHandler(DEVICE_INDEX-1, state);

    game_pad->RegisterStatusHandler(GamePadStatusHandler);
  }
no_gamepad:;
  */

  // Enter the uxn interpreter
  {
    UxnKernel uxn(*this, reinterpret_cast<const u8*>(uxn_rom), sizeof(uxn_rom));
    uxn.run();
  }

  return ShutdownHalt;
}

void RaspiKernel::GamePadStatusHandler(unsigned device_index, const TGamePadState *state) {
  if (device_index != DEVICE_INDEX-1) return;

  assert(instance != 0);
  assert(state != 0);
  memcpy(&instance->game_pad_state, state, sizeof *state);
}

void RaspiKernel::GamePadRemovedHandler(CDevice* device, void* context) {
  assert(instance != 0);

  static const char ClearScreen[] = "\x1b[H\x1b[J\x1b[?25h";
  instance->screen.Write(ClearScreen, sizeof ClearScreen);

  CLogger::Get()->Write(FromKernel, LogDebug, "Gamepad removed");

  instance->game_pad = 0;

  CLogger::Get()->Write(FromKernel, LogNotice, "Please attach an USB gamepad!");
}
