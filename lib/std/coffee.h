/* coffee.h - header for bridging between C and Coffee.
   Copyright (c) 2023 mini-rose */

#pragma once

#include <stdbool.h>

typedef enum
{
	CF_I8,
	CF_U8,
	CF_I16,
	CF_U16,
	CF_I32,
	CF_U32,
	CF_I64,
	CF_U64,
	CF_NULL,
	CF_BOOL
} cf_type;

/* Coffee types */
typedef char cf_i8;
typedef unsigned char cf_u8;
typedef short cf_i16;
typedef unsigned short cf_u16;
typedef int cf_i32;
typedef unsigned int cf_u32;
typedef long cf_i64;
typedef float cf_f32;
typedef double cf_f64;
typedef unsigned long cf_u64;
typedef void cf_null;
typedef bool cf_bool;

#define CF_STR_ALLOC 1

typedef struct
{
	cf_i64 len;
	cf_i8 *ptr;
	cf_i32 flags;
} cf_str;

/* Max functions on the callstack. */
#define CF_STACKLIMIT 2048
