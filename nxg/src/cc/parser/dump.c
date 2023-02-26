/* parser/dump.c - dump expr trees
   Copyright (c) 2023 mini-rose */

#include <nxg/cc/alloc.h>
#include <nxg/cc/parser.h>
#include <nxg/cc/type.h>
#include <nxg/utils/error.h>
#include <nxg/utils/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expr_print_level(expr_t *expr, int level, bool with_next);

static void indent(int levels)
{
	for (int i = 0; i < (levels * 2); i++)
		fputc(' ', stdout);
}

const char *expr_typename(expr_type type)
{
	static const char *names[] = {
	    "SKIP",    "MODULE", "FUNCTION", "CALL",      "RETURN",
	    "VARDECL", "ASSIGN", "VALUE",    "CONDITION", "BLOCK"};

	if (type >= 0 && type < LEN(names))
		return names[type];
	return "<EXPR>";
}

const char *value_expr_type_name(value_expr_type t)
{
	static const char *names[] = {
	    "NULL", "REF",   "LIT",  "CALL", "ADD",    "SUB", "MUL", "DIV",
	    "PTR",  "DEREF", "MREF", "MPTR", "MDEREF", "EQ",  "NEQ", "TUPLE"};

	if (t >= 0 && t < LEN(names))
		return names[t];
	return "<value expr type>";
}

static const char *expr_info(expr_t *expr)
{
	static char info[512];
	assign_expr_t *var;
	mod_expr_t *mod;
	char *tmp = NULL;
	char marker;

	if (!expr)
		return "";

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

		switch (var->to->type) {
		case VE_LIT:
		case VE_REF:
		case VE_MREF:
			marker = 1;
			break;
		case VE_DEREF:
		case VE_MDEREF:
			marker = '*';
			break;
		default:
			marker = '?';
		}

		char *field;
		if (var->to->member) {
			field = slab_alloc(512);
			snprintf(field, 512, ".%s", var->to->member);
		} else {
			field = slab_strdup("");
		}

		snprintf(info, 512, "%c%s%s = (\e[33m%s\e[0m) %s", marker,
			 var->to->name, field, tmp,
			 value_expr_type_name(var->value->type));

		break;
	case E_RETURN:
		tmp = type_name(E_AS_VAL(expr->data)->return_type);
		snprintf(info, 512, "\e[33m%s\e[0m %s", tmp,
			 value_expr_type_name(E_AS_VAL(expr->data)->type));
		break;
	case E_CALL:
		tmp = slab_alloc(128);
		if (E_AS_CALL(expr->data)->object_name) {
			snprintf(tmp, 128, "%s.%s",
				 E_AS_CALL(expr->data)->object_name,
				 E_AS_CALL(expr->data)->name);
		} else {
			snprintf(tmp, 128, "%s", E_AS_CALL(expr->data)->name);
		}

		snprintf(info, 512, "\e[34m%s\e[0m \e[97mn_args=\e[0m%d", tmp,
			 E_AS_CALL(expr->data)->n_args);
		break;
	default:
		info[0] = 0;
	}

	return info;
}

void expr_print_value_expr(value_expr_t *val, int level)
{
	char *lit_str;
	char *tmp = NULL;

	if (val->type == VE_NULL)
		return;

	for (int i = 0; i < level; i++)
		fputs("  ", stdout);

	switch (val->type) {
	case VE_REF:
		printf("ref: `%s`\n", val->name);
		break;
	case VE_LIT:
		lit_str = stringify_literal(val->literal);
		tmp = type_name(val->literal->type);
		printf("literal: \e[33m%s\e[0m %s\n", tmp, lit_str);
		break;
	case VE_CALL:
		printf("call: `%s` n_args=%d\n", val->call->name,
		       val->call->n_args);
		for (int i = 0; i < val->call->n_args; i++)
			expr_print_value_expr(val->call->args[i], level + 1);
		break;
	case VE_PTR:
		printf("addr: `&%s`\n", val->name);
		break;
	case VE_DEREF:
		printf("deref: `*%s`\n", val->name);
		break;
	case VE_MREF:
		printf("member: `%s.%s`\n", val->name, val->member);
		break;
	case VE_MPTR:
		printf("member: `&%s.%s`\n", val->name, val->member);
		break;
	case VE_ADD:
	case VE_SUB:
	case VE_MUL:
	case VE_DIV:
		printf("op: %s\n", value_expr_type_name(val->type));
		if (val->left)
			expr_print_value_expr(val->left, level + 1);
		if (val->right)
			expr_print_value_expr(val->right, level + 1);
		break;
	case VE_EQ:
		printf("comparison `==`:\n");
		expr_print_value_expr(val->left, level + 1);
		expr_print_value_expr(val->right, level + 1);
		break;
	case VE_NEQ:
		printf("comparison `!=`:\n");
		expr_print_value_expr(val->left, level + 1);
		expr_print_value_expr(val->right, level + 1);
		break;
	case VE_TUPLE:
		printf("tuple %s[%d]:\n", type_name(val->tuple->element_type),
		       val->tuple->len);
		for (int i = 0; i < val->tuple->len; i++)
			expr_print_value_expr(val->tuple->values[i], level + 1);
		break;
	default:
		error("dump: cannot dump VE_%s value expr",
		      value_expr_type_name(val->type));
	}
}

static void dump_object_type(object_type_t *o, int level)
{
	indent(level);
	printf("\e[93m%s\e[0m { ", o->name);
	for (int i = 0; i < o->n_fields; i++) {
		printf("%s: \e[93m%s\e[0m ", o->field_names[i],
		       type_name(o->fields[i]));
	}
	printf("}\n");

	for (int i = 0; i < o->n_methods; i++) {
		expr_print_level(o->methods[i], level + 1, true);
	}
}

static void expr_print_mod_expr(mod_expr_t *mod, int level)
{
	char *tmp;

	if (mod->c_objects->n) {
		indent(level);
		printf("\e[95mObjects to link (%d):\e[0m\n", mod->c_objects->n);
	}

	for (int i = 0; i < mod->c_objects->n; i++) {
		indent(level + 1);
		printf("%s\n", mod->c_objects->objects[i]);
	}

	if (mod->n_local_decls) {
		indent(level);
		printf("\e[95mLocal declarations (%d):\e[0m\n",
		       mod->n_local_decls);
	}

	for (int i = 0; i < mod->n_local_decls; i++) {
		indent(level + 1);
		tmp = fn_str_signature(mod->local_decls[i], true);
		printf("%s\n", tmp);
	}

	if (mod->n_decls) {
		indent(level);
		printf("\e[95mExtern declarations (%d):\e[0m\n", mod->n_decls);
	}

	for (int i = 0; i < mod->n_decls; i++) {
		indent(level + 1);
		tmp = fn_str_signature(mod->decls[i], true);
		printf("%s\n", tmp);
	}

	if (mod->n_type_decls) {
		indent(level);
		printf("\e[95mType declarations (%d):\e[0m\n",
		       mod->n_type_decls);
	}

	for (int i = 0; i < mod->n_type_decls; i++) {
		type_t *ty = mod->type_decls[i];

		if (ty->kind == TY_OBJECT) {
			dump_object_type(ty->v_object, level + 1);
		} else {
			indent(level + 1);
			tmp = type_name(mod->type_decls[i]);
			printf("%s\n", tmp);
		}
	}

	if (mod->n_imported) {
		indent(level);
		printf("\e[95mImported modules (%d):\e[0m\n", mod->n_imported);
	}

	for (int i = 0; i < mod->n_imported; i++)
		expr_print_level(mod->imported[i], level + 1, false);

	if (mod->std_modules) {
		if (mod->std_modules->n_modules) {
			indent(level);
			printf("\e[95mStandard modules (%d):\e[0m\n",
			       mod->std_modules->n_modules);
		}

		for (int i = 0; i < mod->std_modules->n_modules; i++) {
			indent(level + 1);
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

	if (expr->type == E_CONDITION) {
		condition_expr_t *cond = expr->data;
		expr_print_value_expr(cond->cond, level + 1);
		indent(level + 1);
		fputs("IF: \n", stdout);
		expr_print_level(cond->if_block, level + 1, true);

		if (cond->else_block) {
			indent(level + 1);
			fputs("ELSE: \n", stdout);
			expr_print_level(cond->else_block, level + 1, true);
		}
	}

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
