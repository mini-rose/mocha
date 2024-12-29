/* mx/type.c - type routines
   Copyright (c) 2023 mini-rose */

#include <mx/cc/alloc.h>
#include <mx/cc/fn_resolve.h>
#include <mx/cc/parser.h>
#include <mx/cc/type.h>
#include <mx/utils/error.h>
#include <mx/utils/utils.h>
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
	for (size_t i = 0; i < LEN(plain_types); i++)
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

	ty = slab_alloc(sizeof(*ty));

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
	char *ty_str = slab_strndup(str, len);
	type_t *ty = type_from_string(ty_str);
	return ty;
}

type_t *type_new()
{
	return slab_alloc(sizeof(type_t));
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
	ty->v_object = slab_alloc(sizeof(*ty->v_object));
	o = ty->v_object;

	/*
	 * obj { len: i64, ptr: &i8, flags: i32 }
	 */

	o->name = slab_strdup("str");
	o->n_fields = 3;
	o->field_names = slab_alloc(3 * sizeof(char *));
	o->fields = slab_alloc(3 * sizeof(type_t *));

	o->field_names[0] = slab_strdup("len");
	o->field_names[1] = slab_strdup("ptr");
	o->field_names[2] = slab_strdup("flags");

	i8 = type_new_plain(PT_I8);

	o->fields[0] = type_new_plain(PT_I64);
	o->fields[1] = type_pointer_of(i8);
	o->fields[2] = type_new_plain(PT_I32);

	return ty;
}

void object_type_add_field(object_type_t *obj, type_t *ty)
{
	obj->fields = realloc_ptr_array(obj->fields, obj->n_fields + 1);
	obj->fields[obj->n_fields++] = ty;
}

static object_type_t *object_type_copy(object_type_t *ty)
{
	object_type_t *new;

	new = slab_alloc(sizeof(*new));
	new->fields = slab_alloc(ty->n_fields * sizeof(type_t *));
	new->methods = slab_alloc(ty->n_methods * sizeof(expr_t *));
	new->field_names = slab_alloc(ty->n_fields * sizeof(char *));
	new->name = slab_strdup(ty->name);

	new->n_fields = ty->n_fields;

	for (int i = 0; i < new->n_fields; i++) {
		new->fields[i] = type_copy(ty->fields[i]);
		new->field_names[i] = slab_strdup(ty->field_names[i]);
	}

	new->n_methods = ty->n_methods;

	for (int i = 0; i < new->n_methods; i++) {
		new->methods[i] = ty->methods[i];
	}

	return new;
}

type_t *type_copy(type_t *ty)
{
	type_t *new_ty;

	new_ty = slab_alloc(sizeof(*new_ty));

	new_ty->kind = ty->kind;

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

fn_candidates_t *type_object_find_fn_candidates(object_type_t *o, char *name)
{
	fn_candidates_t *result;
	fn_expr_t *func;

	result = slab_alloc(sizeof(*result));

	for (int i = 0; i < o->n_methods; i++) {
		func = o->methods[i]->data;

		if (strcmp(func->name, name))
			continue;

		fn_add_candidate(result, func);
	}

	return result;
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
	char *name = slab_alloc(512);
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
	} else if (ty->kind == TY_ARRAY) {
		tmp = type_name(ty->v_base);
		snprintf(name, 512, "%s[]", tmp);
	} else if (is_str_type(ty)) {
		snprintf(name, 512, "str");
	} else if (ty->kind == TY_OBJECT) {
		snprintf(name, 512, "%s", ty->v_object->name);
	} else if (ty->kind == TY_ALIAS) {
		tmp = type_name(ty->v_base);
		snprintf(name, 512, "alias[%s = %s]", ty->alias, tmp);
	} else {
		snprintf(name, 512, "<type>");
	}

	return name;
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

int type_sizeof(type_t *ty)
{
	if (ty->kind != TY_PLAIN)
		error("type: sizeof(<non-plain>) unsupported yet");

	switch (ty->v_plain) {
	case PT_BOOL:
	case PT_I8:
	case PT_U8:
		return 1;
	case PT_I16:
	case PT_U16:
		return 2;
	case PT_I32:
	case PT_U32:
		return 4;
	case PT_I64:
	case PT_U64:
		return 8;
	case PT_I128:
	case PT_U128:
		return 16;
	default:
		return 0;
	}
}

bool type_can_cast(type_t *from, type_t *to)
{
	return from->kind == TY_PLAIN && to->kind == TY_PLAIN;
}

void type_object_add_field(object_type_t *o, char *name, type_t *ty)
{
	o->fields = realloc_ptr_array(o->fields, o->n_fields + 1);
	o->field_names = realloc_ptr_array(o->field_names, o->n_fields + 1);

	o->fields[o->n_fields] = type_copy(ty);
	o->field_names[o->n_fields++] = slab_strdup(name);
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

	return -1;
}

expr_t *type_object_add_method(object_type_t *o)
{
	expr_t *method;

	method = slab_alloc(sizeof(*method));
	o->methods = realloc_ptr_array(o->methods, o->n_methods + 1);
	o->methods[o->n_methods++] = method;

	return method;
}
