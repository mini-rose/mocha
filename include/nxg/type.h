#pragma once

#define bool _Bool

typedef enum
{
	// Pointer to nothing
	T_VOID = 0,

	// Integers
	T_BOOL = 1,
	T_I8 = 2,
	T_I16 = 3,
	T_I32 = 4,
	T_I64 = 5,
	T_I128 = 6,

	// Unsigned integers
	T_U8 = 7,
	T_U16 = 8,
	T_U32 = 9,
	T_U64 = 10,
	T_U128 = 11,

	// Floating point numbers
	T_F32 = 12,
	T_F64 = 13,

	// String
	T_STR = 14,
	T_PTR = 15
} type_t;

/**
 * Checks is string a type
 */
bool is_type(const char *);

/**
 * Get index of type in type enum;
 */
type_t get_type(const char *);
