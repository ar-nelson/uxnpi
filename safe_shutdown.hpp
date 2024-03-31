#pragma once

#include <circle/gpiopin.h>
#include <circle/logger.h>

// based on https://github.com/RetroFlag/retroflag-picase/blob/master/recalbox_SafeShutdown.py

enum class ShutdownMode : u8  {
  None,
  Halt,
  Reboot
};

class SafeShutdown {
  CGPIOManager* manager;
  CLogger* log;
  CGPIOPin reset_pin, power_pin, poweren_pin, led_pin;
  ShutdownMode mode;

  static void poweroff(void* param) {
    auto* self = reinterpret_cast<SafeShutdown*>(param);
    if (self->mode == ShutdownMode::None) {
      self->log->Write("SafeShutdown", LogWarning, "Shutting down...");
      self->led_pin.Write(LOW);
      self->mode = ShutdownMode::Halt;
    }
  }

  static void reset(void* param) {
    auto* self = reinterpret_cast<SafeShutdown*>(param);
    if (self->mode == ShutdownMode::None) {
      self->log->Write("SafeShutdown", LogWarning, "Rebooting...");
      self->mode = ShutdownMode::Reboot;
    }
  }

public:
  SafeShutdown(CGPIOManager* manager, CLogger* log) :
    manager(manager),
    log(log),
    reset_pin(2, TGPIOMode::GPIOModeInputPullUp, manager),
    power_pin(3, TGPIOMode::GPIOModeInputPullUp, manager),
    poweren_pin(4, TGPIOMode::GPIOModeOutput),
    led_pin(14, TGPIOMode::GPIOModeOutput),
    mode(ShutdownMode::None) {}

  bool Initialize() {
    poweren_pin.Write(HIGH);
    led_pin.Write(HIGH);
    reset_pin.ConnectInterrupt(reset, this);
    reset_pin.EnableInterrupt(TGPIOInterrupt::GPIOInterruptOnFallingEdge);
    power_pin.ConnectInterrupt(poweroff, this);
    power_pin.EnableInterrupt(TGPIOInterrupt::GPIOInterruptOnFallingEdge);
    return true;
  }

  ShutdownMode shutdown_mode() const {
    return mode;
  }
};
