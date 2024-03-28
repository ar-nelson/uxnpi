#include "kernel.hpp"
#include <circle/string.h>
#include <circle/util.h>
#include <assert.h>

#define DEVICE_INDEX  1    // "upad1"
#define FILENAME      "tet.rom"

CKernel* CKernel::instance = 0;

static const char FromKernel[] = "kernel";
static const char FromUxn[] = "uxn";

CKernel::CKernel() :
  screen(640, 480), gfx(640, 480),
  timer(&interrupt), logger(options.GetLogLevel(), &timer),
  usb_hci(&interrupt, &timer, TRUE /* TRUE = enable PnP */),
  emmc(&interrupt, &timer, &act_led), game_pad(0)
{
  instance = this;
}

CKernel::~CKernel() {
  instance = 0;
}

bool CKernel::initialize() {
  bool OK = true;

  if (OK) OK = screen.Initialize();
  if (OK) OK = gfx.Initialize();
  if (OK) OK = serial.Initialize(115200);
  if (OK) {
    CDevice* target = device_name_service.GetDevice(options.GetLogDevice(), FALSE);
    OK = logger.Initialize(target ? target : &screen);
  }
  if (OK) OK = interrupt.Initialize();
  if (OK) OK = timer.Initialize();
  if (OK) OK = usb_hci.Initialize();
  if (OK) OK = emmc.Initialize();
  if (OK) {
    if (f_mount(&fs, "SD:", 1) != FR_OK) {
      logger.Write(FromKernel, LogPanic, "Cannot mount drive SD:");
      OK = false;
    }
  }

  if (!OK) logger.Write(FromKernel, LogPanic, "Kernel init failed");
  return OK;
}

ShutdownMode CKernel::run() {
  logger.Write(FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

  // Try to get a gamepad ONCE.

  //gfx.Rotor(0, 0);
  timer.MsDelay(20);

  // This must be called from TASK_LEVEL to update the tree of connected USB devices.
  bool updated = usb_hci.UpdatePlugAndPlay();

  if (game_pad == 0) {
    if (!updated) {
      logger.Write(FromKernel, LogWarning, "No gamepad found");
      goto no_gamepad;
    }

    // get pointer to gamepad device and check if it is supported
    game_pad = (CUSBGamePadDevice*)
      device_name_service.GetDevice("upad", DEVICE_INDEX, FALSE);
    if (game_pad == 0) goto no_gamepad;

    game_pad->RegisterRemovedHandler(GamePadRemovedHandler);

    if (!(game_pad->GetProperties() & GamePadPropertyIsKnown)) {
      logger.Write(FromKernel, LogWarning, "Gamepad mapping is not known");
      goto no_gamepad;
    }

    // get initial state from gamepad and register status handler
    const TGamePadState* state = game_pad->GetInitialState();
    assert(state != 0);
    GamePadStatusHandler(DEVICE_INDEX-1, state);

    game_pad->RegisterStatusHandler(GamePadStatusHandler);
  }
no_gamepad:;

  // Enter the uxn interpreter
  varvara = new uxn::CircleVarvara(gfx, timer, logger, fs, FILENAME);
  if (!varvara->init()) {
    logger.Write(FromKernel, LogPanic, "Varvara init failed");
  } else {
    varvara->run();
  }

  return ShutdownHalt;
}

void CKernel::GamePadStatusHandler(unsigned device_index, const TGamePadState *state) {
  if (device_index != DEVICE_INDEX - 1) return;

  assert(instance != 0);
  assert(state != 0);
  if (instance->varvara) instance->varvara->game_pad_input(state);
}

void CKernel::GamePadRemovedHandler(CDevice* device, void* context) {
  assert(instance != 0);
  CLogger::Get()->Write(FromKernel, LogDebug, "Gamepad removed");
  instance->game_pad = 0;
}
