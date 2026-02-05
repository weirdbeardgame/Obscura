#pragma once

typedef char s8;
typedef short s16;
typedef int s32;
typedef long s64;

typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;

typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;

typedef union
{
  vs8 s8;
  vs16 s16;
  vs32 s32;
  vs64 s64;
  vu8 u8;
  vu16 u16;
  vu32 u32;
  vu64 u64;
} _VX;

typedef float f32;
