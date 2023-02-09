#include <nxg/cc/module.h>
#include <nxg/cc/parser.h>
#include <nxg/utils/error.h>
#include <stdlib.h>
#include <string.h>

fn_expr_t *module_add_decl(expr_t *module)
{
	mod_expr_t *mod = module->data;

	mod->decls =
	    realloc(mod->decls, sizeof(fn_expr_t *) * (mod->n_decls + 1));
	mod->decls[mod->n_decls] = calloc(1, sizeof(fn_expr_t));
	return mod->decls[mod->n_decls++];
}

fn_expr_t *module_add_local_decl(expr_t *module)
{
	mod_expr_t *mod = module->data;

	mod->local_decls = realloc(
	    mod->local_decls, sizeof(fn_expr_t *) * (mod->n_local_decls + 1));
	mod->local_decls[mod->n_local_decls] = calloc(1, sizeof(fn_expr_t));
	return mod->local_decls[mod->n_local_decls++];
}

void add_candidate(fn_candidates_t *resolved, fn_expr_t *ptr)
{
	resolved->candidate =
	    realloc(resolved->candidate,
		    sizeof(fn_expr_t *) + (resolved->n_candidates + 1));
	resolved->candidate[resolved->n_candidates++] = ptr;
}

fn_candidates_t *module_find_fn_candidates(expr_t *module, char *name)
{
	mod_expr_t *mod = module->data;
	fn_candidates_t *resolved;
	fn_expr_t *fn;
	expr_t *walker;

	resolved = calloc(1, sizeof(*resolved));

	/* check the module itself */
	walker = module->child;
	do {
		if (walker->type != E_FUNCTION)
			continue;
		fn = walker->data;
		if (!strcmp(fn->name, name))
			add_candidate(resolved, fn);
	} while ((walker = walker->next));

	/* Local declarations have priority */
	for (int i = 0; i < mod->n_local_decls; i++) {
		fn = mod->local_decls[i];
		if (!strcmp(fn->name, name))
			add_candidate(resolved, fn);
	}

	/* Extern declarations have less priority, so local functions get
	   resolved first */
	for (int i = 0; i < mod->n_decls; i++) {
		fn = mod->decls[i];
		if (!strcmp(fn->name, name))
			add_candidate(resolved, fn);
	}

	return resolved;
}
