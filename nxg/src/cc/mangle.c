/* nxg/mangle.c
   Copyright (c) 2023 mini-rose */

#include <nxg/cc/mangle.h>
#include <nxg/utils/error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

	if (t >= 0 && t < n)
		return type_mangle_ids[t];
	return 'v';
}

char *mangled_type_str(type_t *ty, char *buf)
{
	int offt;

	if (!buf)
		buf = calloc(512, 1);

	if (ty->type == TY_PLAIN) {
		if (ty->v_plain == PT_STR)
			strcpy(buf, "3str");
		else
			buf[0] = mangled_type_char(ty->v_plain);
	} else if (ty->type == TY_POINTER) {
		buf[0] = 'P';
		mangled_type_str(ty->v_base, &buf[1]);
	} else if (ty->type == TY_ARRAY) {
		buf[0] = 'A';
		offt = sprintf(&buf[1], "%zu_", ty->len);
		mangled_type_str(ty->v_base, &buf[offt + 1]);
	} else if (ty->type == TY_OBJECT) {
		/* parse this as the fully qualified name of the type */
		sprintf(buf, "%zu%s", strlen(ty->v_object->name),
			ty->v_object->name);
	} else {
		error("cannot mangle %s type", type_name(ty));
	}

	return buf;
}

char *nxg_mangle(const fn_expr_t *func)
{
	char *name = calloc(512, 1);

	snprintf(name, 512, "_C%d%s", (int) strlen(func->name), func->name);
	for (int i = 0; i < func->n_params; i++)
		mangled_type_str(func->params[i]->type, &name[strlen(name)]);

	return name;
}
