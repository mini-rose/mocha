/* nxg/mangle.c
   Copyright (c) 2023 mini-rose */

#include <nxg/mangle.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char mangled_type_char(plain_type t)
{
	/* This table is based on the Itanium C++ ABI, apart from PT_STR which
	   we use an upper case 'S' for.
	   https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangle.builtin-type
	 */
	const char type_mangle_ids[] = {
	    [PT_NULL] = 'v', [PT_BOOL] = 'b', [PT_I8] = 'a',   [PT_U8] = 'h',
	    [PT_I16] = 's',  [PT_U16] = 't',  [PT_I32] = 'i',  [PT_U32] = 'j',
	    [PT_I64] = 'l',  [PT_U64] = 'm',  [PT_I128] = 'n', [PT_U128] = 'o',
	    [PT_F32] = 'f',  [PT_F64] = 'd',  [PT_STR] = 'S',  [PT_PTR] = 'p'};
	const int n = sizeof(type_mangle_ids);

	if (t >= 0 && t < n)
		return type_mangle_ids[t];
	return 'v';
}

char *nxg_mangle(const fn_expr_t *func)
{
	char *name = malloc(512);
	memset(name, 0, 512);

	snprintf(name, 512, "_C%d%s", (int) strlen(func->name), func->name);
	for (int i = 0; i < func->n_params; i++)
		name[strlen(name)] = mangled_type_char(func->params[i]->type);

	return name;
}
