#pragma once

#include "circle_varvara.hpp"
#include "safe_shutdown.hpp"
#include <circle/actled.h>
//#include <circle/gpiomanager.h>
//#include <circle/i2cmaster.h>
#include <SDCard/emmc.h>

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

  // TODO: Figure out how to instantiate either of these devices
  // without crashing the GPi Case 2W's GPIO graphics driver
  // and making the graphics glitch out.

  // CGPIOManager        gpio;
  // CI2CMaster          i2c;

  CTimer              timer;
  CLogger             logger;
  CUSBHCIDevice       usb_hci;
  CEMMCDevice         emmc;
  FATFS               fs;

  // SafeShutdown        safe_shutdown;

  CUSBGamePadDevice* volatile game_pad;
  TGamePadState game_pad_state;

  uxn::CircleVarvara* varvara = nullptr;
  static CKernel* instance;
};
