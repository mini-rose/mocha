/* type.h - type definitions
   Copyright (c) 2023 mini-rose */

#pragma once

#include <mx/cc/expr.h>
#include <mx/cc/fn_resolve.h>
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
} plain_type;

typedef struct type type_t;

typedef enum
{
	TY_NULL,    /* null */
	TY_PLAIN,   /* T */
	TY_POINTER, /* &T */
	TY_ARRAY,   /* T[] */
	TY_OBJECT,  /* type T {} */
	TY_ALIAS,   /* type T = U */
} type_kind;

typedef struct
{
	char *name;
	type_t **fields;
	char **field_names;
	int n_fields;
	expr_t **methods;
	int n_methods;
} object_type_t;

struct type
{
	type_kind kind;
	char *alias; /* aliased name */
	union
	{
		plain_type v_plain; /* plain type */
		type_t *v_base;     /* base type of pointer/element/alias */
		object_type_t *v_object; /* object type */
	};
};

bool is_plain_type_from_str(const char *str);

bool type_is_integer(type_t *ty);
bool type_is_float(type_t *ty);
bool type_is_string(type_t *ty);
bool type_is_nullptr(type_t *ty);

const char *plain_type_example_varname(plain_type t);
const char *plain_type_name(plain_type t);

type_t *type_from_string(const char *str);
type_t *type_from_sized_string(const char *str, int len);
type_t *type_new();
type_t *type_new_null();
type_t *type_new_plain(plain_type t);
type_t *type_build_str();
type_t *type_copy(type_t *ty);
type_t *type_pointer_of(type_t *ty);
bool type_cmp(type_t *left, type_t *right);
char *type_name(type_t *ty);
const char *type_example_varname(type_t *ty);
int type_sizeof(type_t *t);

bool type_can_cast(type_t *from_type, type_t *into_type);

void type_object_add_field(object_type_t *o, char *name, type_t *ty);
type_t *type_object_field_type(object_type_t *o, char *name);
int type_object_field_index(object_type_t *o, char *name);
expr_t *type_object_add_method(object_type_t *o);
type_t *type_of_member(type_t *ty, char *member);

fn_candidates_t *type_object_find_fn_candidates(object_type_t *o, char *name);
