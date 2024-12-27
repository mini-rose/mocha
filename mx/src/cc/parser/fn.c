/* parser/fn.c - function utilities
   Copyright (c) 2023 mini-rose */

#include <mx/cc/alloc.h>
#include <mx/cc/parser.h>
#include <stdlib.h>
#include <string.h>

bool fn_sigcmp(fn_expr_t *first, fn_expr_t *other)
{
	if (strcmp(first->name, other->name))
		return false;
	if (first->n_params != other->n_params)
		return false;

	for (int i = 0; i < first->n_params; i++) {
		if (!type_cmp(first->params[i]->type, other->params[i]->type))
			return false;
	}

	return true;
}

void fn_add_param(fn_expr_t *fn, const char *name, int len, type_t *type)
{
	fn->params = realloc_ptr_array(fn->params, ++fn->n_params);
	fn->params[fn->n_params - 1] = slab_alloc(sizeof(var_decl_expr_t));
	fn->params[fn->n_params - 1]->name = slab_strndup(name, len);
	fn->params[fn->n_params - 1]->type = type_copy(type);
}
