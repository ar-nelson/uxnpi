#pragma once

#include "circle_varvara.hpp"
#include <circle/actled.h>
#include <SDCard/emmc.h>

enum ShutdownMode {
  ShutdownNone,
  ShutdownHalt,
  ShutdownReboot
};

class CKernel {
public:
  CKernel();
  ~CKernel();

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
  C2DGraphics         gfx;
  CSerialDevice       serial;
  CExceptionHandler   exception_handler;
  CInterruptSystem    interrupt;
  CTimer              timer;
  CLogger             logger;
  CUSBHCIDevice       usb_hci;
  CEMMCDevice         emmc;
  FATFS               fs;

  CUSBGamePadDevice* volatile game_pad;
  TGamePadState game_pad_state;

  uxn::CircleVarvara* varvara = nullptr;
  static CKernel* instance;
};
