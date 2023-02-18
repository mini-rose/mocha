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
    [PT_F32] = "f32",   [PT_F64] = "f64"};

bool is_plain_type(const char *str)
{
	for (int i = 0; i < LEN(plain_types); i++)
		if (!strcmp(str, plain_types[i]))
			return true;

	return false;
}

bool is_str_type(type_t *ty)
{
	return ty->kind == TY_OBJECT && !strcmp(ty->v_object->name, "str");
}

const char *plain_type_name(plain_type t)
{
	if (t >= 0 && t < LEN(plain_types))
		return plain_types[t];
	return "<non-plain type>";
}

const char *plain_type_example_varname(plain_type t)
{
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

		/* Plain or null T */
		if (!strcmp(str, "null"))
			ty->kind = TY_NULL;
		else
			ty->kind = TY_PLAIN;
		ty->v_plain = i;
		return ty;
	}

	if (*str == '&') {
		ty->kind = TY_POINTER;
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
	ty->kind = TY_NULL;
	return ty;
}

type_t *type_new_plain(plain_type t)
{
	type_t *ty = type_new();
	ty->kind = TY_PLAIN;
	ty->v_plain = t;
	return ty;
}

type_t *type_build_str()
{
	type_t *ty = type_new();
	type_t *i8;
	object_type_t *o;

	ty->kind = TY_OBJECT;
	ty->v_object = calloc(1, sizeof(*ty->v_object));
	o = ty->v_object;

	/*
	 * obj { len: i64, ptr: &i8, ref: i32 }
	 */

	o->name = strdup("str");
	o->n_fields = 3;
	o->field_names = calloc(3, sizeof(char *));
	o->fields = calloc(3, sizeof(type_t *));

	o->field_names[0] = strdup("len");
	o->field_names[1] = strdup("ptr");
	o->field_names[2] = strdup("ref");

	i8 = type_new_plain(PT_I8);

	o->fields[0] = type_new_plain(PT_I64);
	o->fields[1] = type_pointer_of(i8);
	o->fields[2] = type_new_plain(PT_I32);

	type_destroy(i8);

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
	new->fields = calloc(3, sizeof(type_t *));
	new->field_names = calloc(3, sizeof(char *));
	new->name = strdup(ty->name);

	new->n_fields = ty->n_fields;

	for (int i = 0; i < new->n_fields; i++) {
		new->fields[i] = type_copy(ty->fields[i]);
		new->field_names[i] = strdup(ty->field_names[i]);
	}

	return new;
}

type_t *type_copy(type_t *ty)
{
	type_t *new_ty;

	new_ty = calloc(1, sizeof(*new_ty));

	new_ty->kind = ty->kind;
	new_ty->len = ty->len;

	if (ty->kind == TY_POINTER || ty->kind == TY_ARRAY) {
		new_ty->v_base = type_copy(ty->v_base);
	} else if (ty->kind == TY_PLAIN) {
		new_ty->v_plain = ty->v_plain;
	} else if (ty->kind == TY_OBJECT) {
		new_ty->v_object = object_type_copy(ty->v_object);
	} else if (ty->kind == TY_NULL) {
	} else {
		error("failed to copy type %s", type_name(ty));
	}

	return new_ty;
}

type_t *type_pointer_of(type_t *ty)
{
	type_t *ptr = type_new_null();
	ptr->kind = TY_POINTER;
	ptr->v_base = type_copy(ty);
	return ptr;
}

type_t *type_of_member(type_t *ty, char *member)
{
	object_type_t *o = ty->v_object;

	if (ty->kind != TY_OBJECT)
		return type_new_null();

	for (int i = 0; i < o->n_fields; i++) {
		if (strcmp(o->field_names[i], member))
			continue;
		return type_copy(o->fields[i]);
	}

	return type_new_null();
}

bool type_cmp(type_t *left, type_t *right)
{
	if (left == right)
		return true;

	if (left->kind != right->kind)
		return false;

	if (left->kind == TY_PLAIN)
		return left->v_plain == right->v_plain;

	if (left->kind == TY_POINTER)
		return type_cmp(left->v_base, right->v_base);

	if (left->kind == TY_ARRAY) {
		if (left->len != right->len)
			return false;
		return type_cmp(left->v_base, right->v_base);
	}

	if (left->kind == TY_OBJECT) {
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

	if (!ty || ty->kind == TY_NULL) {
		snprintf(name, 512, "null");
		return name;
	}

	if (ty->kind == TY_PLAIN) {
		snprintf(name, 512, "%s", plain_type_name(ty->v_plain));
	} else if (ty->kind == TY_POINTER) {
		tmp = type_name(ty->v_base);
		snprintf(name, 512, "&%s", tmp);
		free(tmp);
	} else if (ty->kind == TY_ARRAY) {
		tmp = type_name(ty->v_base);
		snprintf(name, 512, "%s[%zu]", tmp, ty->len);
		free(tmp);
	} else if (is_str_type(ty)) {
		snprintf(name, 512, "str");
	} else if (ty->kind == TY_OBJECT) {
		snprintf(name, 512, "%s { ", ty->v_object->name);
		for (int i = 0; i < ty->v_object->n_fields; i++) {
			tmp = type_name(ty->v_object->fields[i]);
			strcat(name, tmp);
			strcat(name, " ");
			free(tmp);
		}
		strcat(name, "}");
	} else if (ty->kind == TY_ALIAS) {
		tmp = type_name(ty->v_base);
		snprintf(name, 512, "alias[%s = %s]", ty->alias, tmp);
		free(tmp);
	} else {
		snprintf(name, 512, "<type>");
	}

	return name;
}

void type_destroy(type_t *ty)
{
	if (!ty)
		return;

	if (ty->kind == TY_NULL || ty->kind == TY_PLAIN) {
		/* do nothing */
	} else if (ty->kind == TY_POINTER || ty->kind == TY_ARRAY) {
		type_destroy(ty->v_base);
	} else if (ty->kind == TY_OBJECT) {
		type_object_destroy(ty->v_object);
	} else if (ty->kind == TY_ALIAS) {
		free(ty->alias);
		type_destroy(ty->v_base);
	} else {
		error("type: cannot destroy type %s", type_name(ty));
	}

	free(ty);
}

void type_object_destroy(object_type_t *obj)
{
	for (int i = 0; i < obj->n_fields; i++) {
		type_destroy(obj->fields[i]);
		free(obj->field_names[i]);
	}

	free(obj->fields);
	free(obj->field_names);
	free(obj->name);
	free(obj);
}

const char *type_example_varname(type_t *ty)
{
	if (ty->kind == TY_PLAIN)
		return plain_type_example_varname(ty->v_plain);
	if (ty->kind == TY_POINTER)
		return "ptr";
	if (ty->kind == TY_ARRAY)
		return "array";
	if (ty->kind == TY_OBJECT)
		return "object";
	return "x";
}

void type_object_add_field(object_type_t *o, char *name, type_t *ty)
{
	o->fields = realloc(o->fields, sizeof(type_t *) * (o->n_fields + 1));
	o->field_names =
	    realloc(o->field_names, sizeof(char *) * (o->n_fields + 1));

	o->fields[o->n_fields] = type_copy(ty);
	o->field_names[o->n_fields++] = strdup(name);
}

type_t *type_object_field_type(object_type_t *o, char *name)
{
	for (int i = 0; i < o->n_fields; i++) {
		if (strcmp(o->field_names[i], name))
			continue;
		return type_copy(o->fields[i]);
	}

	return type_new_null();
}

int type_object_field_index(object_type_t *o, char *name)
{
	for (int i = 0; i < o->n_fields; i++) {
		if (strcmp(o->field_names[i], name))
			continue;
		return i;
	}

	return 0;
}
