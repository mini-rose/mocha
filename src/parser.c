#include <nxg/error.h>
#include <nxg/parser.h>
#include <stdlib.h>
#include <string.h>

static void mod_expr_free(mod_expr_t *module)
{
	free(module->name);
}

static void fn_expr_free(fn_expr_t *function)
{
	free(function->name);
}

static token *next_tok(const token_list *list)
{
	static const token_list *self = NULL;
	static int current = -1;

	return (self == NULL)
		? (self = list)->tokens[0]
		: self->tokens[++current];
}

static expr_t *expr_add_child(expr_t *parent)
{
	expr_t *node = calloc(sizeof(*node), 1);
	expr_t *walker = parent->child;

	if (!parent->child)
		return parent->child = node;

	/* find the first empty slot in the linked list */
	while (walker->next)
		walker = walker->next;

	walker->next = node;
	return node;
}

#define TOK_IS(TOK, TYPE, VALUE)                                               \
 (((TOK)->type == (TYPE)) && !strncmp((TOK)->value, VALUE, (TOK)->len))

static err_t parse_fn(token_list *tokens, expr_t *parent)
{
	fn_expr_t *data;
	expr_t *node;
	token *tok;

	node = expr_add_child(parent);
	data = calloc(sizeof(*data), 1);

	node->type = E_FUNCTION;
	node->data = data;
	node->data_free = (expr_free_handle) fn_expr_free;

	tok = next_tok(NULL);

	/* name */
	if (tok->type != T_IDENT) {
		error_at(tokens->source->content, tok->value,
			 "expected function name, got %s", tokname(tok->type));
		return ERR_SYNTAX;
	}

	data->name = strndup(tok->value, tok->len);

	/* parameters (currently skip) */
	tok = next_tok(NULL);
	if (!TOK_IS(tok, T_PUNCT, "(")) {
		error_at(tokens->source->content, tok->value,
			 "expected `(` after function name");
		return ERR_SYNTAX;
	}

	tok = next_tok(NULL);
	while (!TOK_IS(tok, T_PUNCT, ")")) {
		if (tok->type == T_END) {
			error_at(tokens->source->content, tok->value,
				 "end of function parameter list for `%s()` "
				 "function not found",
				 data->name);
			return ERR_SYNTAX;
		}

		tok = next_tok(NULL);
	}

	/* return type (optional) */
	tok = next_tok(NULL);
	if (TOK_IS(tok, T_PUNCT, ":")) {
		tok = next_tok(NULL);
		if (tok->type != T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "missing function return type");
			return ERR_SYNTAX;
		}

		/* TODO: set data->return_type */
	}

	/* opening & closing braces */
	tok = next_tok(NULL);
	if (!TOK_IS(tok, T_PUNCT, "{")) {
		error_at(tokens->source->content, tok->value,
			 "missing opening brace for `%s` function", data->name);
		return ERR_SYNTAX;
	}

	/* TODO: closing brace, all expr inside. */

	return 0;
}

expr_t *parse(token_list *tokens)
{
	token *current = next_tok(tokens);
	expr_t *module = calloc(sizeof(*module), 1);
	char *content = tokens->source->content;

	module->type = E_MODULE;
	module->data = malloc(sizeof(mod_expr_t));
	module->data_free = (expr_free_handle) mod_expr_free;
	E_AS_MOD(module->data)->name = strdup("__main__");

	token_list_print(tokens);

	while ((current = next_tok(NULL)) && current->type != T_END) {
		/* top-level: function decl */
		if (current->type == T_KEYWORD
		    && !strncmp(current->value, "fn", current->len)) {
			if (parse_fn(tokens, module))
				goto err;
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
