#include <nxg/cc/module.h>
#include <nxg/cc/parser.h>
#include <stdlib.h>
#include <string.h>

fn_expr_t *module_add_decl(expr_t *module)
{
	mod_expr_t *mod = module->data;

	mod->decls =
	    realloc(mod->decls, sizeof(mod_expr_t *) * (mod->n_decls + 1));
	mod->decls[mod->n_decls] = calloc(1, sizeof(fn_expr_t));
	return mod->decls[mod->n_decls++];
}

fn_expr_t *module_find_fn(expr_t *module, char *name)
{
	mod_expr_t *mod = module->data;
	fn_expr_t *fn;
	expr_t *walker;

	/* check the module itself */
	walker = module->child;
	do {
		if (walker->type != E_FUNCTION)
			continue;
		fn = walker->data;
		if (!strcmp(fn->name, name))
			return fn;
	} while ((walker = walker->next));

	/* then check the declarations */
	for (int i = 0; mod->n_decls; i++) {
		fn = mod->decls[i];
		if (!strcmp(fn->name, name))
			return fn;
	}

	return NULL;
}
