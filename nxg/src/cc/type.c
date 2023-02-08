#include <nxg/cc/type.h>
#include <nxg/utils/error.h>
#include <nxg/utils/utils.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *plain_types[] = {
    [0] = "null",       [PT_BOOL] = "bool", [PT_I8] = "i8",
    [PT_I16] = "i16",   [PT_I32] = "i32",   [PT_I64] = "i64",
    [PT_I128] = "i128", [PT_U8] = "u8",     [PT_U16] = "u16",
    [PT_U32] = "u32",   [PT_U64] = "u64",   [PT_U128] = "u128",
    [PT_F32] = "f32",   [PT_F64] = "f64",   [PT_STR] = "str"};

bool is_plain_type(const char *str)
{
	for (int i = 0; i < LEN(plain_types); i++)
		if (!strcmp(str, plain_types[i]))
			return true;

	return false;
}

const char *plain_type_name(plain_type t)
{
	if (t >= 0 && t < LEN(plain_types))
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

	for (size_t i = 0; i < LEN(plain_types); i++) {
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

type_t *type_from_sized_string(const char *str, int len)
{
	char *ty_str = strndup(str, len);
	type_t *ty = type_from_string(ty_str);
	free(ty_str);
	return ty;
}

type_t *type_new()
{
	return calloc(1, sizeof(type_t));
}

type_t *type_new_null()
{
	type_t *ty = type_new();
	ty->type = TY_NULL;
	return ty;
}

type_t *type_new_plain(plain_type t)
{
	type_t *ty = type_new();
	ty->type = TY_PLAIN;
	ty->v_plain = t;
	return ty;
}

void object_type_add_field(object_type_t *obj, type_t *ty)
{
	obj->fields =
	    realloc(obj->fields, sizeof(type_t *) * (obj->n_fields + 1));
	obj->fields[obj->n_fields++] = ty;
}

static object_type_t *object_type_copy(object_type_t *ty)
{
	object_type_t *new;

	new = calloc(1, sizeof(*new));

	new->n_fields = ty->n_fields;
	for (int i = 0; i < ty->n_fields; i++)
		object_type_add_field(new, type_copy(ty->fields[i]));

	return new;
}

type_t *type_copy(type_t *ty)
{
	type_t *new_ty;

	new_ty = calloc(1, sizeof(*new_ty));
	memcpy(new_ty, ty, sizeof(*ty));

	if (ty->type == TY_POINTER || ty->type == TY_ARRAY) {
		new_ty->v_base = type_copy(ty->v_base);
	} else if (ty->type == TY_OBJECT) {
		new_ty->v_object = object_type_copy(ty->v_object);
	}

	return new_ty;
}

type_t *type_pointer_of(type_t *ty)
{
	type_t *ptr = type_new_null();
	ptr->type = TY_POINTER;
	ptr->v_base = type_copy(ty);
	return ptr;
}

bool type_cmp(type_t *left, type_t *right)
{
	if (left == right)
		return true;

	if (left->type != right->type)
		return false;

	if (left->type == TY_PLAIN)
		return left->v_plain == right->v_plain;

	if (left->type == TY_POINTER)
		return type_cmp(left->v_base, right->v_base);

	if (left->type == TY_ARRAY) {
		if (left->len != right->len)
			return false;
		return type_cmp(left->v_base, right->v_base);
	}

	if (left->type == TY_OBJECT) {
		if (left->v_object->n_fields != right->v_object->n_fields)
			return false;

		if (strcmp(left->v_object->name, right->v_object->name))
			return false;

		for (int i = 0; i < left->v_object->n_fields; i++) {
			if (!type_cmp(left->v_object->fields[i],
				      right->v_object->fields[i])) {
				return false;
			}
		}

		return true;
	}

	return false;
}

char *type_name(type_t *ty)
{
	char *name = calloc(512, 1);
	char *tmp;

	if (!ty || ty->type == TY_NULL) {
		snprintf(name, 512, "null");
		return name;
	}

	if (ty->type == TY_PLAIN) {
		snprintf(name, 512, "%s",
			 ty->v_plain <= PT_STR ? plain_types[ty->v_plain]
					       : "<plain type>");
	} else if (ty->type == TY_POINTER) {
		tmp = type_name(ty->v_base);
		snprintf(name, 512, "&%s", tmp);
		free(tmp);
	} else if (ty->type == TY_ARRAY) {
		tmp = type_name(ty->v_base);
		snprintf(name, 512, "%s[%zu]", tmp, ty->len);
		free(tmp);
	} else if (ty->type == TY_OBJECT) {
		snprintf(name, 512, "%s { ", ty->v_object->name);
		for (int i = 0; i < ty->v_object->n_fields; i++) {
			tmp = type_name(ty->v_object->fields[i]);
			strcat(name, tmp);
			strcat(name, " ");
			free(tmp);
		}
		strcat(name, "}");
	}

	return name;
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
