#include "kernel.hpp"
#include <circle/startup.h>

int main(void) {
  // cannot return here because some destructors used in RaspiKernel are not implemented

  RaspiKernel Kernel;
  if (!Kernel.initialize()) {
    halt();
    return EXIT_HALT;
  }
  auto ShutdownMode = Kernel.run();

  switch (ShutdownMode) {
  case ShutdownReboot:
    reboot();
    return EXIT_REBOOT;
  case ShutdownHalt:
  default:
    halt();
    return EXIT_HALT;
  }
}
