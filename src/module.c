#include <nxg/module.h>
#include <nxg/parser.h>
#include <string.h>

fn_expr_t *module_find_fn(expr_t *module, char *name)
{
	fn_expr_t *fn;
	expr_t *walker;

	walker = module->child;
	do {
		if (walker->type != E_FUNCTION)
			continue;
		fn = walker->data;
		if (!strcmp(fn->name, name))
			return fn;
	} while ((walker = walker->next));

	return NULL;
}
