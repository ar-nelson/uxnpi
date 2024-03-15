#pragma once

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbgamepad.h>
#include <circle/types.h>
#include "ppu.hpp"
#include "uxn.h"

enum ShutdownMode {
  ShutdownNone,
  ShutdownHalt,
  ShutdownReboot
};

class RaspiKernel;

class UxnKernel {
public:
  UxnKernel(RaspiKernel& kernel, const u8* rom, u16 rom_size);
  ~UxnKernel();

  bool run();
private:
  static void nil_talk(Device* d, u8 b0, u8 w);
  static void console_talk(Device* d, u8 b0, u8 w);
  static void system_talk(Device* d, u8 b0, u8 w);
  static void screen_talk(Device* d, u8 b0, u8 w);
  static void audio_talk(Device* d, u8 b0, u8 w);
  static void datetime_talk(Device* d, u8 b0, u8 w);
  static void file_talk(Device* d, u8 b0, u8 w);

  Uxn u;
  Ppu ppu;
  Device *devscreen, *devctrl, *devmouse, *devaudio;
  CTimer& timer;
  CLogger& logger;

  static UxnKernel* instance;
};

class RaspiKernel {
public:
  RaspiKernel();
  ~RaspiKernel();

  bool initialize();

  ShutdownMode run();

  friend class UxnKernel;

private:
  static void GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState* pState);
  static void GamePadRemovedHandler(CDevice* pDevice, void* pContext);

private:
  // do not change this order
  CActLED             act_led;
  CKernelOptions      options;
  CDeviceNameService  device_name_service;
  CScreenDevice       screen;
  CSerialDevice       serial;
  CExceptionHandler   exception_handler;
  CInterruptSystem    interrupt;
  CTimer              timer;
  CLogger             logger;
  CUSBHCIDevice       usb_hci;

  CUSBGamePadDevice* volatile game_pad;
  bool game_pad_known;
  TGamePadState game_pad_state;

  int pos_x, pos_y;

  static RaspiKernel* instance;
};
