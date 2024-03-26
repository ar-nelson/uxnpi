#pragma once
#include "varvara.hpp"
#include <iostream>

namespace uxn {

class StdlibConsole : public Console {
public:
  StdlibConsole(Uxn& uxn) : Console(uxn) {}

  void write_byte(u8 b) final {
    if (b == '\n') std::cout << std::endl;
    else std::cout << static_cast<char>(b);
  }

  void flush() {
    std::flush(std::cout);
  }
};

}
