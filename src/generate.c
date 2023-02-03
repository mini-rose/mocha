#include <llvm-c/Core.h>
#include <nxg/generate.h>
#include <nxg/parser.h>
#include <stdlib.h>

var_t *var_new()
{
	return (var_t *) calloc(1, sizeof(var_t));
}

void var_destroy(var_t *var)
{
	if (var->name)
		free(var->name);

	if (var->value)
		free(var->value);

	free(var);
}

func_t *func_new()
{
	return (func_t *) calloc(1, sizeof(func_t));
}

void func_destroy(func_t *func)
{
	int i;

	for (i = 0; i < func->n_args; i++)
		free(func->args[i]);

	for (i = 0; i < func->n_locals; i++)
		free(func->locals[i]);

	free(func);
}

void func_add_var(func_t *func, var_t *var)
{
	func->locals = realloc(func->locals, ++func->n_locals * sizeof(var_t));
	func->locals[func->n_locals - 1] = var;
}

void func_add_arg(func_t *func, arg_t *arg)
{
	func->args = realloc(func->locals, ++func->n_args * sizeof(arg_t));
	func->args[func->n_args - 1] = arg;
}

module_t *module_new()
{
	return (module_t *) calloc(1, sizeof(module_t));
}

void generate(expr_t *expr) { }
