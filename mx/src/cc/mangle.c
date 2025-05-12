/* mx/mangle.c - mangle functions and types
   Copyright (c) 2023 mini-rose */

#include <mx/cc/alloc.h>
#include <mx/cc/mangle.h>
#include <mx/utils/error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MANGLE_BUF_SIZ 1024

static char mangled_type_char(plain_type t)
{
	/* This table is based on the Itanium C++ ABI
	   https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangle.builtin-type
	 */
	static const char type_mangle_ids[] = {
	    [PT_NULL] = 'v', [PT_BOOL] = 'b', [PT_I8] = 'a',   [PT_U8] = 'h',
	    [PT_I16] = 's',  [PT_U16] = 't',  [PT_I32] = 'i',  [PT_U32] = 'j',
	    [PT_I64] = 'l',  [PT_U64] = 'm',  [PT_I128] = 'n', [PT_U128] = 'o',
	    [PT_F32] = 'f',  [PT_F64] = 'd'};
	static const int n = sizeof(type_mangle_ids);

	if ((int) t < n)
		return type_mangle_ids[t];
	return 'v';
}

char *mx_mangle_type(type_t *ty, char *buf)
{
	if (!buf)
		buf = slab_alloc(MANGLE_BUF_SIZ);

	if (ty->kind == TY_PLAIN) {
		buf[0] = mangled_type_char(ty->v_plain);
	} else if (ty->kind == TY_POINTER) {
		buf[0] = 'P';
		mx_mangle_type(ty->v_base, &buf[1]);
	} else if (ty->kind == TY_ARRAY) {
		snprintf(buf, MANGLE_BUF_SIZ, "A_");
		mx_mangle_type(ty->v_base, &buf[2]);
	} else if (ty->kind == TY_OBJECT) {
		/* parse this as the fully qualified name of the type */
		snprintf(buf, MANGLE_BUF_SIZ, "%zu%s", strlen(ty->v_object->name),
			ty->v_object->name);
	} else {
		error("cannot mangle %s type", type_name(ty));
	}

	return buf;
}

char *mx_mangle(const fn_expr_t *func)
{
	char *name = slab_alloc(MANGLE_BUF_SIZ);

	if (func->object) {
		snprintf(name, MANGLE_BUF_SIZ, "_MN%d%s%d%sE",
			 (int) strlen(func->object->name), func->object->name,
			 (int) strlen(func->name), func->name);
	} else {
		snprintf(name, MANGLE_BUF_SIZ, "_M%d%s",
			 (int) strlen(func->name), func->name);
	}

	for (int i = 0; i < func->n_params; i++)
		mx_mangle_type(func->params[i]->type, &name[strlen(name)]);

	if (!func->n_params)
		name[strlen(name)] = mangled_type_char(PT_NULL);

	return name;
}
