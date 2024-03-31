#include "kernel.hpp"
#include <circle/string.h>
#include <circle/util.h>
#include <circle/sound/pwmsoundbasedevice.h>
//#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <assert.h>

#define DEVICE_INDEX  1    // "upad1"
#define FILENAME      "launcher.rom"

CKernel* CKernel::instance = 0;

static const char FromKernel[] = "kernel";

CKernel::CKernel() :
  screen(640, 480), gfx(640, 480),
  // gpio(&interrupt),
  // i2c(CMachineInfo::Get()->GetDevice(DeviceI2CMaster), TRUE),
  timer(&interrupt), logger(options.GetLogLevel(), &timer),
  usb_hci(&interrupt, &timer, TRUE /* TRUE = enable PnP */),
  emmc(&interrupt, &timer, &act_led),
  // safe_shutdown(&gpio, &logger),
  game_pad(0)
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
  //if (OK) OK = gpio.Initialize();
  //if (OK) OK = i2c.Initialize();
  if (OK) OK = timer.Initialize();
  if (OK) OK = usb_hci.Initialize();
  if (OK) OK = emmc.Initialize();
  //if (OK) OK = safe_shutdown.Initialize();
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

  // Initialize sound
  const char* sound_device = options.GetSoundDevice();
  CSoundBaseDevice* sound = nullptr;
  constexpr unsigned SAMPLE_RATE = 48000; // overall system clock
  constexpr unsigned CHUNK_SIZE = 384 * 10; // number of samples, written to sound device at once
  if (strcmp (sound_device, "sndpwm") == 0) {
    logger.Write("Audio", LogNotice, "Found PWM sound device");
    sound = new CPWMSoundBaseDevice(&interrupt, SAMPLE_RATE, CHUNK_SIZE);
  } else if (strcmp (sound_device, "sndi2s") == 0) {
    // TODO: Figure out how to make this device work with the Case 2W.
    //logger.Write("Audio", LogNotice, "Found I2S sound device");
    //sound = new CI2SSoundBaseDevice(&interrupt, SAMPLE_RATE, CHUNK_SIZE, FALSE, &i2c, 0);
  } else if (strcmp (sound_device, "sndhdmi") == 0) {
    logger.Write("Audio", LogNotice, "Found HDMI sound device");
    sound = new CHDMISoundBaseDevice(&interrupt, SAMPLE_RATE, CHUNK_SIZE);
  } else {
    logger.Write("Audio", LogWarning, "No sound device found!");
  }

  // Enter the uxn interpreter
  auto shutdown_mode = ShutdownMode::Halt;
  varvara = new uxn::CircleVarvara(gfx, nullptr, timer, logger, fs, FILENAME);
  if (!varvara->init()) {
    logger.Write(FromKernel, LogPanic, "Varvara init failed");
  } else {
    shutdown_mode = varvara->run(/*&safe_shutdown*/);
  }

  if (sound_device) delete sound_device;
  return shutdown_mode;
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
