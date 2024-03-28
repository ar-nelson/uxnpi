#pragma once
#include "shorthand.h"

namespace uxn {

static constexpr u16 PAGE_PROGRAM = 0x100;

static inline u16 peek2(u8* d) {
  return (*(d) << 8 | (d)[1]);
}

static inline void poke2(u8* d, u16 v) {
  *(d) = (v) >> 8; (d)[1] = (v);
}

struct Slice {
  const u8* data;
  u16 size;

  u8 operator[](u16 i) const { return data[i % size]; }
};

struct MutableSlice {
  u8* data;
  u16 size;

  u8& operator[](u16 i) { return data[i % size]; }
  operator Slice() const { return { data, size }; }
};

// Each memory array has 1 more byte than necessary, to prevent
// a peek2 on the highest address from reading out of bounds.

struct Bank {
  u8 mem[0x10001] = {0};
};

class BankIndex2 {
  Bank* banks[0x100] = {0};
public:
  Bank& operator[](u8 ix) {
    if (!banks[ix]) banks[ix] = new Bank;
    return *banks[ix];
  }
  ~BankIndex2() {
    for (size_t i = 0; i < 0x100; i++) if (banks[i]) delete banks[i];
  }
};

class BankIndex1 {
  BankIndex2* banks[0x100] = {0};
public:
  BankIndex2& operator[](u8 ix) {
    if (!banks[ix]) banks[ix] = new BankIndex2;
    return *banks[ix];
  }
  ~BankIndex1() {
    for (size_t i = 0; i < 0x100; i++) if (banks[i]) delete banks[i];
  }
};

struct Stack {
  u8 dat[0x101], ptr;
};

struct Uxn {
  const u8* boot_rom;
  u32 boot_rom_size;
  u8 ram[0x10001], dev[0x101];
  BankIndex1* banks;
  Stack wst, rst;
  bool initialized = false;

  Uxn(const u8* rom, u32 rom_size) : boot_rom(rom), boot_rom_size(rom_size), banks(nullptr) {}
  virtual ~Uxn() { if (banks) delete banks; }

  virtual bool init();
  virtual void reset(bool soft = false);

  bool eval(u16 pc);
  bool call_vec(u8 d) {
    u16 addr = peek2(dev + d);
    return addr ? eval(addr) : false;
  }

  u8* bank(u16 index) {
    if (index == 0) return ram;
    if (!banks) banks = new BankIndex1;
    return (*banks)[index << 8][index & 0xff].mem;
  }

  Slice null_terminated_string_in_ram(u16 addr) const {
    u16 end;
    for (end = addr; end >= addr && ram[end] != 0; end++) {}
    return { ram + addr, static_cast<u16>(end < addr ? 0 : end - addr) };
  }
  Slice bounded_range_in_ram(u16 addr, u16 length) const {
    return { ram + addr, addr + length > 0xffff ? static_cast<u16>(0x10000 - addr) : length };
  }
  MutableSlice bounded_range_in_ram_mutable(u16 addr, u16 length) {
    return { ram + addr, addr + length > 0xffff ? static_cast<u16>(0x10000 - addr) : length };
  }

  virtual void before_dei(u8 d) = 0;
  virtual void after_deo(u8 d) = 0;

protected:
  Uxn() : Uxn(nullptr, 0) {}
};

}
