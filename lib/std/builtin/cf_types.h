/* cf_types.h - C representations of Coffee types
   Copyright (c) 2023 mini-rose */

#pragma once

typedef char cf_i8;
typedef unsigned char cf_u8;
typedef short cf_i16;
typedef unsigned short cf_u16;
typedef int cf_i32;
typedef unsigned int cf_u32;
typedef long cf_i64;
typedef unsigned long cf_u64;
typedef void cf_null;

struct cf_str
{
	cf_i64 len;
	cf_i8 *ptr;
	cf_i32 ref;
};
