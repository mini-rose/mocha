#pragma once

#define bool _Bool

typedef enum
{
	PT_NULL = 0,

	// Integers
	PT_BOOL = 1,
	PT_I8 = 2,
	PT_I16 = 3,
	PT_I32 = 4,
	PT_I64 = 5,
	PT_I128 = 6,

	// Unsigned integers
	PT_U8 = 7,
	PT_U16 = 8,
	PT_U32 = 9,
	PT_U64 = 10,
	PT_U128 = 11,

	// Floating point numbers
	PT_F32 = 12,
	PT_F64 = 13,

	// String
	PT_STR = 14,
	PT_PTR = 15
} plain_type;

/**
 * Checks is string a type
 */
bool is_plain_type(const char *str);

/**
 * Get index of type in type enum;
 */
plain_type plain_type_from(const char *str, int len);

const char *plain_type_example_varname(plain_type t);
const char *plain_type_name(plain_type t);
