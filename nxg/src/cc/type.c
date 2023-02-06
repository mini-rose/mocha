#include <nxg/cc/type.h>
#include <nxg/utils/error.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define LENGTH(array) sizeof(array) / sizeof(*array)

static char *plain_types[] = {
    [PT_NULL] = "null", [PT_BOOL] = "bool", [PT_I8] = "i8",
    [PT_I16] = "i16",   [PT_I32] = "i32",   [PT_I64] = "i64",
    [PT_I128] = "i128", [PT_U8] = "u8",     [PT_U16] = "u16",
    [PT_U32] = "u32",   [PT_U64] = "u64",   [PT_U128] = "u128",
    [PT_F32] = "f32",   [PT_F64] = "f64",   [PT_STR] = "str"};

static const size_t n_plain_types = LENGTH(plain_types);

bool is_plain_type(const char *str)
{
	for (int i = 0; i < n_plain_types; i++)
		if (!strcmp(str, plain_types[i]))
			return true;

	return false;
}

plain_type plain_type_from(const char *str, int len)
{
	for (int i = 0; i < n_plain_types; i++) {
		if (!strncmp(str, plain_types[i], len))
			return i;
	}

	return PT_NULL;
}

const char *plain_type_name(plain_type t)
{
	if (t >= 0 && t < n_plain_types)
		return plain_types[t];
	return "<non-plain type>";
}

const char *plain_type_example_varname(plain_type t)
{
	if (t == PT_STR)
		return "string";
	if (t >= PT_I8 && t <= PT_U64)
		return "number";
	if (t == PT_BOOL)
		return "is_something";
	return "x";
}

type_t *type_from_string(const char *str)
{
	type_t *ty;

	ty = calloc(1, sizeof(*ty));

	for (size_t i = 0; i < n_plain_types; i++) {
		if (strcmp(plain_types[i], str))
			continue;

		/* plain T */
		ty->type = TY_PLAIN;
		ty->v_plain = i;
		return ty;
	}

	if (*str == '&') {
		ty->type = TY_POINTER;
		ty->v_base = type_from_string(str + 1);
		return ty;
	}

	return ty;
}

void type_destroy(type_t *ty)
{
	if (ty->type == TY_POINTER || ty->type == TY_ARRAY)
		type_destroy(ty->v_base);
	if (ty->type == TY_OBJECT) {
		for (int i = 0; i < ty->v_object->n_fields; i++)
			type_destroy(ty->v_object->fields[i]);
		free(ty->v_object->fields);
	}

	free(ty);
}

const char *type_example_varname(type_t *ty)
{
	if (ty->type == TY_PLAIN)
		return plain_type_example_varname(ty->v_plain);
	if (ty->type == TY_POINTER)
		return "ptr";
	if (ty->type == TY_ARRAY)
		return "array";
	if (ty->type == TY_OBJECT)
		return "object";
	return "x";
}
