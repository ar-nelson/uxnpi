#pragma once
/*
Copyright (c) 2021 Bad Diode

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#include <stddef.h>
#include <stdint.h>

typedef uint8_t      u8;
typedef uint16_t     u16;
typedef uint32_t     u32;
typedef uint64_t     u64;
typedef int8_t       s8;
typedef int16_t      s16;
typedef int32_t      s32;
typedef int64_t      s64;
typedef volatile u8  vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;
typedef volatile s8  vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;

#define KB(N) ((u64)(N)   * 1024)
#define MB(N) ((u64)KB(N) * 1024)
#define GB(N) ((u64)MB(N) * 1024)
#define TB(N) ((u64)GB(N) * 1024)
