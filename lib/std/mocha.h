/* mocha.h - header for bridging between C and Mocha.
   Copyright (c) 2023 mini-rose */

#pragma once

#include <stdbool.h>

typedef enum
{
	MOCHA_I8,
	MOCHA_U8,
	MOCHA_I16,
	MOCHA_U16,
	MOCHA_I32,
	MOCHA_U32,
	MOCHA_I64,
	MOCHA_U64,
	MOCHA_STR,
	MOCHA_NULL,
	MOCHA_BOOL
} mo_type;

/* Mocha types */
typedef char mo_i8;
typedef unsigned char mo_u8;
typedef short mo_i16;
typedef unsigned short mo_u16;
typedef int mo_i32;
typedef unsigned int mo_u32;
typedef long mo_i64;
typedef float mo_f32;
typedef double mo_f64;
typedef unsigned long mo_u64;
typedef void mo_null;
typedef bool mo_bool;

#define MOCHA_STR_ALLOC 1

typedef struct
{
	mo_i64 len;
	mo_i8 *ptr;
	mo_i32 flags;
} mo_str;

/* Max functions on the callstack. */
#define MOCHA_STACKLIMIT 2048
