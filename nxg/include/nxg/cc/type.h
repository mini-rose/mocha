/* type.h - type definitions
   Copyright (c) 2023 mini-rose */

#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef enum
{
	// Undefined type
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

	// String is a plain type from the language perspective, and a struct
	// type when compiling.
	PT_STR = 14,
} plain_type;

typedef struct type type_t;

typedef enum
{
	TY_NULL,    /* null */
	TY_PLAIN,   /* T */
	TY_POINTER, /* &T */
	TY_ARRAY,   /* T[] */
	TY_OBJECT,  /* obj T {} */
} type_type;

typedef struct
{
	char *name;
	type_t **fields;
	char **field_names;
	int n_fields;
} object_type_t;

struct type
{
	type_type type;
	size_t len; /* in case of array type */
	union
	{
		plain_type v_plain;      /* plain type */
		type_t *v_base;          /* base type of pointer/element */
		object_type_t *v_object; /* object type */
	};
};

/**
 * Checks is string a type
 */
bool is_plain_type(const char *str);

const char *plain_type_example_varname(plain_type t);
const char *plain_type_name(plain_type t);

type_t *type_from_string(const char *str);
type_t *type_from_sized_string(const char *str, int len);
type_t *type_new();
type_t *type_new_null();
type_t *type_new_plain(plain_type t);
type_t *type_copy(type_t *ty);
type_t *type_pointer_of(type_t *ty);
bool type_cmp(type_t *left, type_t *right);
char *type_name(type_t *ty);
void type_destroy(type_t *ty);
const char *type_example_varname(type_t *ty);
