/* parser/dump.c - dump expr trees
   Copyright (c) 2023 mini-rose */

#include "nxg/utils/error.h"

#include <nxg/cc/parser.h>
#include <stdio.h>
#include <stdlib.h>

static const char *expr_info(expr_t *expr)
{
	static char info[512];
	assign_expr_t *var;
	mod_expr_t *mod;
	char *tmp = NULL;
	char marker;

	switch (expr->type) {
	case E_MODULE:
		mod = expr->data;
		snprintf(info, 512, "\e[1;98m%s\e[0m src=%s", mod->name,
			 mod->source_name);
		break;
	case E_FUNCTION:
		tmp = fn_str_signature(E_AS_FN(expr->data), true);
		snprintf(info, 512, "%s", tmp);
		break;
	case E_VARDECL:
		tmp = type_name(E_AS_VDECL(expr->data)->type);
		snprintf(info, 512, "\e[33m%s\e[0m %s", tmp,
			 E_AS_VDECL(expr->data)->name);
		break;
	case E_ASSIGN:
		var = E_AS_ASS(expr->data);
		tmp = type_name(var->value->return_type);

		if (var->to->type == VE_LIT || var->to->type == VE_REF)
			marker = ' ';
		else if (var->to->type == VE_PTR)
			marker = '&';
		else if (var->to->type == VE_DEREF)
			marker = '*';
		else
			marker = '?';

		snprintf(info, 512, "%c%s = (\e[33m%s\e[0m) %s", marker,
			 var->to->name, tmp,
			 value_expr_type_name(var->value->type));
		break;
	case E_RETURN:
		tmp = type_name(E_AS_VAL(expr->data)->return_type);
		snprintf(info, 512, "\e[33m%s\e[0m %s", tmp,
			 value_expr_type_name(E_AS_VAL(expr->data)->type));
		break;
	case E_CALL:
		snprintf(info, 512, "\e[34m%s\e[0m \e[97mn_args=\e[0m%d",
			 E_AS_CALL(expr->data)->name,
			 E_AS_CALL(expr->data)->n_args);
		break;
	default:
		info[0] = 0;
	}

	free(tmp);
	return info;
}

static void expr_print_level(expr_t *expr, int level, bool with_next);

static void expr_print_value_expr(value_expr_t *val, int level)
{
	char *lit_str;
	char *tmp = NULL;

	for (int i = 0; i < level; i++)
		fputs("  ", stdout);

	if (val->type == VE_NULL) {
		printf("literal: \e[33mnull\e[0m\n");
	} else if (val->type == VE_REF) {
		printf("ref: `%s`\n", val->name);
	} else if (val->type == VE_LIT) {
		lit_str = stringify_literal(val->literal);
		tmp = type_name(val->literal->type);
		printf("literal: \e[33m%s\e[0m %s\n", tmp, lit_str);
		free(lit_str);
	} else if (val->type == VE_CALL) {
		printf("call: `%s` n_args=%d\n", val->call->name,
		       val->call->n_args);
		for (int i = 0; i < val->call->n_args; i++)
			expr_print_value_expr(val->call->args[i], level + 1);
	} else if (val->type == VE_PTR) {
		printf("addr: `&%s`\n", val->name);
	} else if (val->type == VE_DEREF) {
		printf("deref: `*%s`\n", val->name);
	} else {
		printf("op: %s\n", value_expr_type_name(val->type));
		if (val->left)
			expr_print_value_expr(val->left, level + 1);
		if (val->right)
			expr_print_value_expr(val->right, level + 1);
	}

	free(tmp);
}

static void expr_print_mod_expr(mod_expr_t *mod, int level)
{
	char *tmp;

	if (mod->c_objects->n) {
		indent(0, 2 * (level));
		printf("\e[95mObjects to link (%d):\e[0m\n", mod->c_objects->n);
	}

	for (int i = 0; i < mod->c_objects->n; i++) {
		indent(0, 2 * (level + 1));
		printf("%s\n", mod->c_objects->objects[i]);
	}

	if (mod->n_local_decls) {
		indent(0, 2 * (level));
		printf("\e[95mLocal declarations (%d):\e[0m\n",
		       mod->n_local_decls);
	}

	for (int i = 0; i < mod->n_local_decls; i++) {
		indent(0, 2 * (level + 1));
		tmp = fn_str_signature(mod->local_decls[i], true);
		printf("%s\n", tmp);
		free(tmp);
	}

	if (mod->n_decls) {
		indent(0, 2 * (level));
		printf("\e[95mExtern declarations (%d):\e[0m\n", mod->n_decls);
	}

	for (int i = 0; i < mod->n_decls; i++) {
		indent(0, 2 * (level + 1));
		tmp = fn_str_signature(mod->decls[i], true);
		printf("%s\n", tmp);
		free(tmp);
	}

	if (mod->n_type_decls) {
		indent(0, 2 * (level));
		printf("\e[95mType declarations (%d):\e[0m\n",
		       mod->n_type_decls);
	}

	for (int i = 0; i < mod->n_type_decls; i++) {
		indent(0, 2 * (level + 1));
		tmp = type_name(mod->type_decls[i]);
		printf("%s\n", tmp);
		free(tmp);
	}

	if (mod->n_imported) {
		indent(0, 2 * (level));
		printf("\e[95mImported modules (%d):\e[0m\n", mod->n_imported);
	}

	for (int i = 0; i < mod->n_imported; i++)
		expr_print_level(mod->imported[i], level + 1, false);

	if (mod->std_modules) {
		if (mod->std_modules->n_modules) {
			indent(0, 2 * (level));
			printf("\e[95mStandard modules (%d):\e[0m\n",
			       mod->std_modules->n_modules);
		}

		for (int i = 0; i < mod->std_modules->n_modules; i++) {
			indent(0, 2 * (level + 1));
			printf(
			    "%s\n",
			    E_AS_MOD(mod->std_modules->modules[i]->data)->name);
		}
	}
}

static void expr_print_level(expr_t *expr, int level, bool with_next)
{
	expr_t *walker;

	for (int i = 0; i < level; i++)
		fputs("  ", stdout);

	printf("\e[%sm%s\e[0m %s\n", expr->type == E_MODULE ? "1;91" : "96",
	       expr_typename(expr->type), expr_info(expr));

	if (expr->type == E_RETURN)
		expr_print_value_expr(expr->data, level + 1);

	if (expr->type == E_MODULE)
		expr_print_mod_expr(expr->data, level + 1);

	if (expr->type == E_ASSIGN)
		expr_print_value_expr(E_AS_ASS(expr->data)->value, level + 1);

	if (expr->type == E_CALL) {
		for (int i = 0; i < E_AS_CALL(expr->data)->n_args; i++) {
			expr_print_value_expr(E_AS_CALL(expr->data)->args[i],
					      level + 1);
		}
	}

	if (expr->child)
		expr_print_level(expr->child, level + 1, true);

	if (with_next) {
		walker = expr;
		while (walker && (walker = walker->next))
			expr_print_level(walker, level, false);
	}
}

void expr_print(expr_t *expr)
{
	expr_print_level(expr, 0, true);
}
