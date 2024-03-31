#include "kernel.hpp"
#include <circle/startup.h>

int main(void) {
  // cannot return here because some destructors used in CKernel are not implemented

  CKernel Kernel;
  if (!Kernel.initialize()) {
    halt();
    return EXIT_HALT;
  }
  auto mode = Kernel.run();

  switch (mode) {
    case ShutdownMode::Reboot:
      reboot();
      return EXIT_REBOOT;
    case ShutdownMode::Halt:
    default:
      halt();
      return EXIT_HALT;
  }
}
