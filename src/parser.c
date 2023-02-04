#include <ctype.h>
#include <nxg/error.h>
#include <nxg/parser.h>
#include <nxg/tokenize.h>
#include <nxg/type.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define TOK_IS(TOK, TYPE, VALUE)                                               \
 (((TOK)->type == (TYPE)) && !strncmp((TOK)->value, VALUE, (TOK)->len))

static void mod_expr_free(mod_expr_t *module)
{
	free(module->name);
	free(module->source_name);
}

static void fn_expr_free(fn_expr_t *function)
{
	int i;

	free(function->name);

	for (i = 0; i < function->n_params; i++) {
		free(function->params[i]->name);
		free(function->params[i]);
	}

	free(function->locals);
	free(function->params);
}

static void value_expr_free(value_expr_t *value)
{
	if (value->type == VE_NULL)
		return;
	else if (value->type == VE_REF)
		free(value->name);
	else if (value->type == VE_LIT) {
		if (value->literal->type == PT_STR)
			free(value->literal->v_str.ptr);
		free(value->literal);
	} else {
		expr_destroy(value->left);
		expr_destroy(value->right);
	}
}

static void assign_expr_free(assign_expr_t *assign)
{
	free(assign->name);
	value_expr_free(&assign->value);
}

static void var_decl_expr_free(var_decl_expr_t *variable)
{
	free(variable->name);
}

static token *index_tok(token_list *list, int index)
{
	static token end_token = {.type = T_END, .value = "", .len = 0};

	if (list->iter >= list->length)
		return &end_token;
	return list->tokens[index - 1];
}

static token *next_tok(token_list *list)
{
	return index_tok(list, list->iter++);
}

static expr_t *expr_add_next(expr_t *expr)
{
	expr_t *node = calloc(sizeof(*node), 1);

	while (expr->next)
		expr = expr->next;

	expr->next = node;
	return node;
}

static expr_t *expr_add_child(expr_t *parent)
{
	if (parent->child)
		return expr_add_next(parent->child);

	expr_t *node = calloc(sizeof(*node), 1);
	parent->child = node;
	return node;
}

static void fn_add_param(fn_expr_t *fn, token *name, plain_type type)
{
	fn->params =
	    realloc(fn->params, sizeof(var_decl_expr_t) * ++fn->n_params);
	fn->params[fn->n_params - 1] = malloc(sizeof(var_decl_expr_t));
	fn->params[fn->n_params - 1]->name = strndup(name->value, name->len);
	fn->params[fn->n_params - 1]->type = type;
}

var_decl_expr_t *node_resolve_local(expr_t *node, const char *name, int len)
{
	if (len == 0)
		len = strlen(name);

	/* only support functions now */
	if (node->type != E_FUNCTION)
		return NULL;

	fn_expr_t *fn = node->data;

	for (int i = 0; i < fn->n_params; i++) {
		if (!strncmp(fn->params[i]->name, name, len))
			return fn->params[i];
	}

	for (int i = 0; i < fn->n_locals; i++) {
		if (!strncmp(fn->locals[i]->name, name, len))
			return fn->locals[i];
	}

	return NULL;
}

bool node_has_named_local(expr_t *node, const char *name, int len)
{
	return node_resolve_local(node, name, len) != NULL;
}

/**
 * name: type
 */
static bool is_var_decl(token_list *tokens, token *tok)
{
	if (tok->type != T_IDENT)
		return false;

	tok = index_tok(tokens, tokens->iter);
	if (!TOK_IS(tok, T_PUNCT, ":"))
		return false;

	tok = index_tok(tokens, tokens->iter + 1);
	if (tok->type != T_DATATYPE)
		return false;

	return true;
}

/**
 * literal ::= string | integer | float | "null"
 */
static bool is_literal(token_list *tokens, token *tok)
{
	return tok->type == T_NUMBER || tok->type == T_STRING
	    || TOK_IS(tok, T_DATATYPE, "null");
}

static bool is_integer(token *tok)
{
	for (int i = 0; i < tok->len; i++) {
		if (!isdigit(tok->value[i]))
			return false;
	}

	return true;
}

static bool is_float(token *tok)
{
	bool found_dot = false;
	for (int i = 0; i < tok->len; i++) {
		if (tok->value[i] == '.') {
			if (!found_dot)
				found_dot = true;
			else
				return false;
			continue;
		}

		if (!isdigit(tok->value[i]))
			return false;
	}

	return true;
}

static void parse_literal(value_expr_t *node, token_list *tokens, token *tok)
{
	/* string */
	if (tok->type == T_STRING) {
		node->return_type = PT_STR;
		node->type = VE_LIT;
		node->literal->type = PT_STR;
		node->literal->v_str.ptr = strndup(tok->value, tok->len);
		node->literal->v_str.len = tok->len;
	}

	if (tok->type == T_NUMBER) {
		node->type = VE_LIT;
		char *tmp = strndup(tok->value, tok->len);

		if (is_integer(tok)) {
			node->literal->type = PT_I32;
			node->literal->v_i32 = strtol(tmp, NULL, 10);
		} else if (is_float(tok)) {
			node->literal->type = PT_F32;
			node->literal->v_f32 = strtof(tmp, NULL);
		}

		free(tmp);
	}

	/* "null" */
	if (TOK_IS(tok, T_DATATYPE, "null")) {
		node->return_type = PT_NULL;
		node->type = VE_NULL;
	}
}

/**
 * literal   ::= string | integer | float | "null"
 * reference ::= identifier
 * value     ::= literal | reference | value operator value
 *
 * value
 */
static err_t parse_value_expr(expr_t *context, value_expr_t *node,
			      token_list *tokens, token *tok)
{
	/* literal */
	if (is_literal(tokens, tok)
	    && index_tok(tokens, tokens->iter)->type == T_NEWLINE) {
		node->type = VE_LIT;
		node->literal = calloc(1, sizeof(*node->literal));
		parse_literal(node, tokens, tok);
		node->return_type = node->literal->type;
	}

	/* reference */
	if (tok->type == T_IDENT
	    && index_tok(tokens, tokens->iter)->type == T_NEWLINE) {
		if (!node_has_named_local(context, tok->value, tok->len)) {
			error_at(tokens->source->content, tok->value,
				 "use of undeclared variable: `%.*s`", tok->len,
				 tok->value);
		}

		node->type = VE_REF;
		node->name = strndup(tok->value, tok->len);

		var_decl_expr_t *var =
		    node_resolve_local(context, tok->value, tok->len);
		node->return_type = var->type;
	}

	/* TODO: value operator value */

	return ERR_OK;
}

/**
 * name[: type] = value
 */
static bool is_var_assign(token_list *tokens, token *tok)
{
	int offset = 0;

	if (tok->type != T_IDENT)
		return false;

	tok = index_tok(tokens, tokens->iter + offset);
	if (TOK_IS(tok, T_PUNCT, ":")) {
		offset++;
		tok = index_tok(tokens, tokens->iter + offset);
		if (tok->type != T_DATATYPE)
			return false;
		offset++;
	}

	tok = index_tok(tokens, tokens->iter + offset);
	if (!TOK_IS(tok, T_OPERATOR, "="))
		return false;

	return true;
}

static void fn_add_local_var(fn_expr_t *func, var_decl_expr_t *var)
{
	func->locals = realloc(func->locals, sizeof(var_decl_expr_t *)
						 * (func->n_locals + 1));
	func->locals[func->n_locals++] = var;
}

/**
 * name: type
 */
static err_t parse_var_decl(expr_t *parent, fn_expr_t *fn, token_list *tokens,
			    token *tok)
{
	var_decl_expr_t *data;
	expr_t *node;

	if (node_has_named_local(parent, tok->value, tok->len)) {
		error_at(tokens->source->content, tok->value,
			 "a variable named `%.*s` already has been declared in "
			 "this scope",
			 tok->len, tok->value);
	}

	data = calloc(sizeof(*data), 1);
	data->name = strndup(tok->value, tok->len);

	tok = next_tok(tokens);
	tok = next_tok(tokens);
	data->type = plain_type_from(tok->value, tok->len);

	node = expr_add_child(parent);
	node->type = E_VARDECL;
	node->data = data;
	node->data_free = (expr_free_handle) var_decl_expr_free;

	fn_add_local_var(fn, data);

	return ERR_OK;
}

/**
 * name[: type] = value
 */
static err_t parse_assign(expr_t *parent, fn_expr_t *fn, token_list *tokens,
			  token *tok)
{
	assign_expr_t *data;
	expr_t *node;
	token *name;

	name = tok;

	/* defined variable type, meaning we declare a new variable */
	if (is_var_decl(tokens, tok)) {
		parse_var_decl(parent, fn, tokens, tok);
	} else if (!node_has_named_local(parent, tok->value, tok->len)) {
		error_at(tokens->source->content, tok->value,
			 "use of undeclared variable: `%.*s`", tok->len,
			 tok->value);
	}

	node = expr_add_child(parent);
	data = calloc(1, sizeof(*data));

	data->name = strndup(name->value, name->len);
	node->type = E_ASSIGN;
	node->data = data;
	node->data_free = (expr_free_handle) assign_expr_free;

	/* parse value */
	tok = next_tok(tokens);
	tok = next_tok(tokens);

	parse_value_expr(parent, &data->value, tokens, tok);

	return ERR_OK;
}

/**
 * ret [value]
 */
static err_t parse_return(expr_t *parent, token_list *tokens, token *tok)
{
	plain_type return_type;
	value_expr_t *data;
	expr_t *node;

	node = expr_add_child(parent);
	data = calloc(1, sizeof(*data));

	node->type = E_RETURN;
	node->data = data;
	node->data_free = (expr_free_handle) value_expr_free;

	return_type = PT_NULL;
	if (parent->type == E_FUNCTION)
		return_type = E_AS_FN(parent->data)->return_type;

	tok = next_tok(tokens);
	if (tok->type == T_NEWLINE && return_type != PT_NULL) {
		error_at(tokens->source->content, tok->value,
			 "missing return value for function that returns `%s`",
			 plain_type_name(return_type));
	}

	if (tok->type != T_NEWLINE && return_type == PT_NULL) {
		error_at(
		    tokens->source->content, tok->value,
		    "cannot return a value from a function returning null");
	}

	/* return without a value */
	if (return_type == PT_NULL) {
		data->return_type = PT_NULL;
		data->type = VE_NULL;
		return ERR_OK;
	}

	/* return with a value */
	parse_value_expr(parent, data, tokens, tok);

	if (data->return_type != E_AS_FN(parent->data)->return_type) {
		error_at(tokens->source->content, tok->value,
			 "mismatched return type: expression returns %s, while "
			 "the function returns %s",
			 plain_type_name(data->return_type),
			 plain_type_name(E_AS_FN(parent->data)->return_type));
	}

	return ERR_OK;
}

static bool fn_has_return(expr_t *func)
{
	expr_t *walker = func->child;
	if (!walker)
		return false;

	do {
		if (walker->type == E_RETURN)
			return true;
	} while ((walker = walker->next));

	return false;
}

static err_t parse_fn(token_list *tokens, expr_t *module)
{
	fn_expr_t *data;
	expr_t *node;
	token *tok;
	token *name;
	token *return_type_tok;

	node = expr_add_child(module);
	data = calloc(sizeof(*data), 1);

	node->type = E_FUNCTION;
	node->data = data;
	node->data_free = (expr_free_handle) fn_expr_free;
	data->n_params = 0;
	data->return_type = PT_NULL;

	tok = next_tok(tokens);

	/* name */
	if (tok->type != T_IDENT) {
		error_at(tokens->source->content, tok->value,
			 "expected function name, got %s", tokname(tok->type));
		return ERR_SYNTAX;
	}

	name = tok;
	data->name = strndup(tok->value, tok->len);

	/* parameters */
	tok = next_tok(tokens);
	if (!TOK_IS(tok, T_PUNCT, "(")) {
		error_at(tokens->source->content, tok->value,
			 "expected `(` after function name");
		return ERR_SYNTAX;
	}

	tok = next_tok(tokens);
	while (!TOK_IS(tok, T_PUNCT, ")")) {
		plain_type type;
		token *name;

		if (tok->type == T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "the parameter name comes first - the type is "
				 "located after a colon like this: `%s: %.*s`",
				 plain_type_example_varname(
				     plain_type_from(tok->value, tok->len)),
				 tok->len, tok->value);
		}

		if (tok->type != T_IDENT) {
			error_at(tokens->source->content, tok->value,
				 "missing parameter name");
		}

		name = tok;
		tok = next_tok(tokens);

		if (tok->type == T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "you missed a colon between the name and "
				 "type, try `%.*s: %.*s`",
				 name->len, name->value, tok->len, tok->value);
		}

		if (!TOK_IS(tok, T_PUNCT, ":")) {
			error_at(tokens->source->content, name->value,
				 "the `%.*s` parameter is missing a type, try "
				 "`%.*s: i32`",
				 name->len, name->value, name->len,
				 name->value);
		}

		tok = next_tok(tokens);

		if (tok->type != T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "unknown type `%.*s`", tok->len, tok->value);
		}

		type = plain_type_from(tok->value, tok->len);
		tok = next_tok(tokens);

		if (!TOK_IS(tok, T_PUNCT, ",") && !TOK_IS(tok, T_PUNCT, ")")) {
			error_at(tokens->source->content, tok->value,
				 "unexpected token, this should be either a "
				 "comma `,` or a closing parenthesis `)`");
		}

		fn_add_param(data, name, type);

		if (TOK_IS(tok, T_PUNCT, ","))
			tok = next_tok(tokens);
	}

	/* return type (optional) */
	tok = next_tok(tokens);
	return_type_tok = NULL;
	if (TOK_IS(tok, T_PUNCT, ":")) {
		tok = next_tok(tokens);

		if (tok->type != T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "expected return type, got `%.*s`", tok->len,
				 tok->value);
			return ERR_SYNTAX;
		}

		data->return_type = plain_type_from(tok->value, tok->len);
		return_type_tok = tok;
		tok = next_tok(tokens);
	}

	/* opening & closing braces */
	while (tok->type == T_NEWLINE)
		tok = next_tok(tokens);

	if (!TOK_IS(tok, T_PUNCT, "{")) {
		error_at(tokens->source->content, tok->value,
			 "missing opening brace for `%s`", data->name);
		return ERR_SYNTAX;
	}

	int brace_level = 1;
	while (brace_level != 0) {
		tok = next_tok(tokens);

		if (tok->type == T_END) {
			if (brace_level == 1)
				error_at(tokens->source->content, tok->value,
					 "missing a closing brace");
			else
				error_at(tokens->source->content, tok->value,
					 "missing %d closing braces",
					 brace_level);
		}

		if (tok->type == T_NEWLINE)
			continue;

		if (TOK_IS(tok, T_PUNCT, "{")) {
			brace_level++;
			continue;
		}

		if (TOK_IS(tok, T_PUNCT, "}")) {
			brace_level--;
			continue;
		}

		/* statements inside the function */

		if (is_var_assign(tokens, tok))
			parse_assign(node, data, tokens, tok);
		else if (is_var_decl(tokens, tok))
			parse_var_decl(node, data, tokens, tok);
		else if (TOK_IS(tok, T_KEYWORD, "ret"))
			parse_return(node, tokens, tok);
		else
			error_at(tokens->source->content, tok->value,
				 "unparsable");
	}

	/* always add a return statement */
	if (!fn_has_return(node)) {
		if (data->return_type != PT_NULL) {
			error_at(tokens->source->content, tok->value,
				 "missing return statement for %s", data->name);
		}

		expr_t *ret_expr = expr_add_child(node);
		value_expr_t *val = calloc(1, sizeof(*val));

		ret_expr->type = E_RETURN;
		ret_expr->data_free = (expr_free_handle) value_expr_free;
		ret_expr->data = val;
		val->type = VE_NULL;
		val->return_type = PT_NULL;
	}

	if (!strcmp(data->name, "main")) {
		if (data->n_params) {
			error_at(
			    tokens->source->content, name->value,
			    "the main function does not take any arguments");
		}

		if (data->return_type != PT_NULL) {
			error_at(
			    tokens->source->content, return_type_tok->value,
			    "the main function cannot return `%.*s`, as it "
			    "always returns `null`",
			    return_type_tok->len, return_type_tok->value);
		}
	}

	return 0;
}

expr_t *parse(token_list *tokens, const char *module_id)
{
	token *current = next_tok(tokens);
	expr_t *module = calloc(sizeof(*module), 1);
	mod_expr_t *data;
	char *content = tokens->source->content;

	data = malloc(sizeof(mod_expr_t));
	data->name = strdup(module_id);
	data->source_name = strdup(tokens->source->path);

	module->type = E_MODULE;
	module->data = data;
	module->data_free = (expr_free_handle) mod_expr_free;

	while ((current = next_tok(tokens)) && current->type != T_END) {
		/* top-level: function decl */
		if (current->type == T_KEYWORD
		    && !strncmp(current->value, "fn", current->len)) {
			if (parse_fn(tokens, module))
				goto err;
		} else if (current->type == T_NEWLINE) {
			continue;
		} else {
			error_at(content, current->value,
				 "only functions are allowed at the top-level");
			goto err;
		}
	}

err:
	return module;
}

void expr_destroy(expr_t *expr)
{
	if (!expr)
		return;

	if (expr->child)
		expr_destroy(expr->child);
	if (expr->next)
		expr_destroy(expr->next);

	if (expr->data) {
		if (expr->data_free)
			expr->data_free(expr->data);
		free(expr->data);
	}

	free(expr);
}

static const char *expr_typename(expr_type type)
{
	static const char *names[] = {"MODULE",  "FUNCTION", "CALL",   "RETURN",
				      "VARDECL", "ASSIGN",   "LITERAL"};
	static const int n_names = sizeof(names) / sizeof(*names);

	if (type >= 0 && type < n_names)
		return names[type];
	return "<EXPR>";
}

static const char *func_str_signature(fn_expr_t *func)
{
	static char sig[1024];
	char buf[64];
	memset(sig, 0, 1024);

	snprintf(sig, 1024, "%s(", func->name);

	for (int i = 0; i < func->n_params - 1; i++) {
		snprintf(buf, 64, "%s: %s, ", func->params[i]->name,
			 plain_type_name(func->params[i]->type));
		strcat(sig, buf);
	}

	if (func->n_params > 0) {
		snprintf(
		    buf, 64, "%s: %s", func->params[func->n_params - 1]->name,
		    plain_type_name(func->params[func->n_params - 1]->type));
		strcat(sig, buf);
	}

	snprintf(buf, 64, "): %s", plain_type_name(func->return_type));
	strcat(sig, buf);

	return sig;
}

static const char *expr_info(expr_t *expr)
{
	static char info[512];
	assign_expr_t *var;
	char *buf;

	switch (expr->type) {
	case E_MODULE:
		snprintf(info, 512, "%s", E_AS_MOD(expr->data)->name);
		break;
	case E_FUNCTION:
		snprintf(info, 512, "%s",
			 func_str_signature(E_AS_FN(expr->data)));
		break;
	case E_VARDECL:
		snprintf(info, 512, "%s %s",
			 plain_type_name(E_AS_VDECL(expr->data)->type),
			 E_AS_VDECL(expr->data)->name);
		break;
	case E_ASSIGN:
		var = E_AS_ASS(expr->data);
		snprintf(info, 512, "%s = (%s) {}", var->name,
			 plain_type_name(var->value.return_type));
		break;
	case E_LITERAL:
		buf = stringify_literal(E_AS_LIT(expr->data));
		snprintf(info, 512, "%s: %s", buf,
			 plain_type_name(E_AS_LIT(expr->data)->type));
		free(buf);
		break;
	case E_RETURN:
		snprintf(info, 512, "%s %s",
			 plain_type_name(E_AS_VAL(expr->data)->return_type),
			 value_expr_type_name(E_AS_VAL(expr->data)->type));
		break;
	default:
		info[0] = 0;
	}

	return info;
}

static void expr_print_level(expr_t *expr, int level, bool with_next);

static void expr_print_value_expr(expr_t *expr, int level)
{
	value_expr_t *val = expr->data;

	for (int i = 0; i < level; i++)
		fputs("  ", stdout);

	if (val->type == VE_NULL) {
		printf("literal: null\n");
	} else if (val->type == VE_REF) {
		printf("ref: `%s`\n", val->name);
	} else if (val->type == VE_LIT) {
		printf("literal: %s\n", plain_type_name(val->literal->type));
	} else {
		printf("value:\n");
		if (val->left)
			expr_print_level(val->left, level + 1, true);
		if (val->right)
			expr_print_level(val->right, level + 1, true);
	}
}

static void expr_print_level(expr_t *expr, int level, bool with_next)
{
	expr_t *walker;

	for (int i = 0; i < level; i++)
		fputs("  ", stdout);

	printf("%s %s\n", expr_typename(expr->type), expr_info(expr));

	if (expr->type == E_RETURN)
		expr_print_value_expr(expr, level + 1);

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

void literal_default(literal_expr_t *literal)
{
	plain_type t = literal->type;

	switch (t) {
	case PT_STR:
		literal->v_str.len = 0;
		literal->v_str.ptr = "";
		break;
	default:
		memset(literal, 0, sizeof(*literal));
		literal->type = t;
	}
}

char *stringify_literal(literal_expr_t *literal)
{
	if (literal->type == PT_I32) {
		char *buf = malloc(16);
		snprintf(buf, 16, "%d", literal->v_i32);
		return buf;
	}

	if (literal->type == PT_STR)
		return strndup(literal->v_str.ptr, literal->v_str.len);

	if (literal->type == PT_NULL) {
		char *buf = malloc(5);
		strcpy(buf, "null");
		return buf;
	}

	return NULL;
}

const char *value_expr_type_name(value_expr_type t)
{
	static const char *names[] = {"NULL", "REF", "LIT", "ADD",
				      "SUB",  "MUL", "DIV"};
	static const int n_names = sizeof(names) / sizeof(*names);

	if (t >= 0 && t < n_names)
		return names[t];
	return "<value expr type>";
}
