#include "nxg/cc/type.h"

#include <nxg/cc/parser.h>
#include <nxg/utils/error.h>
#include <stdlib.h>

void mod_expr_free(mod_expr_t *module)
{
	for (int i = 0; i < module->n_decls; i++) {
		fn_expr_free(module->decls[i]);
		free(module->decls[i]);
	}

	for (int i = 0; i < module->n_imported; i++)
		expr_destroy(module->imported[i]);

	for (int i = 0; i < module->n_imported; i++)
		free(module->c_objects[i]);

	free(module->c_objects);
	free(module->imported);
	free(module->name);
	free(module->source_name);
	free(module->local_decls);
	free(module->decls);
}

void fn_expr_free(fn_expr_t *function)
{
	int i;

	free(function->name);

	for (i = 0; i < function->n_params; i++) {
		type_destroy(function->params[i]->type);
		free(function->params[i]->name);
		free(function->params[i]);
	}

	type_destroy(function->return_type);
	free(function->locals);
	free(function->params);
}

void literal_expr_free(literal_expr_t *lit)
{
	if (lit->type->kind == TY_OBJECT) {
		free(lit->v_str.ptr);
	}

	type_destroy(lit->type);
}

void value_expr_free(value_expr_t *value)
{
	if (!value)
		return;

	if (value->type == VE_NULL)
		return;
	else if (value->type == VE_REF || value->type == VE_PTR
		 || value->type == VE_DEREF) {
		free(value->name);
	} else if (value->type == VE_LIT) {
		literal_expr_free(value->literal);
		free(value->literal);
	} else if (value->type == VE_CALL) {
		call_expr_free(value->call);
		free(value->call);
	} else {
		value_expr_free(value->left);
		free(value->left);
		value_expr_free(value->right);
		free(value->right);
	}

	type_destroy(value->return_type);
}

void call_expr_free(call_expr_t *call)
{
	free(call->name);
	for (int i = 0; i < call->n_args; i++) {
		value_expr_free(call->args[i]);
		free(call->args[i]);
	}
	free(call->args);
}

void assign_expr_free(assign_expr_t *assign)
{
	value_expr_free(assign->value);
	value_expr_free(assign->to);
	free(assign->to);
	free(assign->value);
}

void var_decl_expr_free(var_decl_expr_t *variable)
{
	type_destroy(variable->type);
	free(variable->name);
}
