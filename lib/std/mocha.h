/* mocha.h - header for bridging between C and Mocha.
   Copyright (c) 2023 mini-rose */

#pragma once

#include <stdbool.h>
#include <string.h>

/* Mocha types */
typedef bool i1;
typedef char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;
typedef long i64;
typedef float f32;
typedef double f64;
typedef unsigned long u64;
typedef void null;

typedef struct
{
	i64 len;
	i8 *ptr;
    bool heap;
} str;
