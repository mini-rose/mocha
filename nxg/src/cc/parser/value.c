#include <nxg/cc/parser.h>
#include <stdlib.h>
#include <string.h>

static void parse_reference(value_expr_t *node, expr_t *context,
			    token_list *tokens, token *tok)
{
	if (!node_has_named_local(context, tok->value, tok->len)) {
		error_at(tokens->source, tok->value, tok->len,
			 "use of undeclared variable");
	}

	node->type = VE_REF;
	node->name = strndup(tok->value, tok->len);

	var_decl_expr_t *var =
	    node_resolve_local(context, tok->value, tok->len);
	node->return_type = type_copy(var->type);
}

err_t parse_single_value(expr_t *context, expr_t *mod, value_expr_t *node,
			 token_list *tokens, token *tok)
{
	type_t *temp_type;

	/* literal */
	if (is_literal(tok)) {
		parse_literal(node, tokens, tok);
		return ERR_OK;
	}

	/* call */
	if (is_call(tokens, tok)) {
		node->type = VE_CALL;
		node->call = calloc(1, sizeof(*node->call));
		parse_inline_call(context, mod, node->call, tokens, tok);
		node->return_type = type_copy(node->call->func->return_type);
		return ERR_OK;
	}

	/* reference */
	if (is_reference(tok)) {
		parse_reference(node, context, tokens, tok);
		return ERR_OK;
	}

	/* dereference */
	if (is_dereference(tokens, tok)) {
		tok = next_tok(tokens);
		parse_reference(node, context, tokens, tok);
		node->type = VE_DEREF;
		temp_type = node->return_type;
		node->return_type = type_copy(node->return_type->v_base);
		type_destroy(temp_type);
		return ERR_OK;
	}

	/* pointer-to */
	if (is_pointer_to(tokens, tok)) {
		tok = next_tok(tokens);
		parse_reference(node, context, tokens, tok);
		node->type = VE_PTR;
		temp_type = node->return_type;
		node->return_type = type_pointer_of(node->return_type);
		type_destroy(temp_type);
		return ERR_OK;
	}

	return ERR_SYNTAX;
}

static int twoside_find_op(token_list *tokens, token *tok)
{
	int offset = 0;

	while (!is_operator(tok))
		tok = index_tok(tokens, tokens->iter + offset++);
	return tokens->iter + (offset - 1);
}

static value_expr_t *parse_twoside_value_expr(expr_t *context, expr_t *mod,
					      value_expr_t *node,
					      token_list *tokens, token *tok)
{
	token *left, *op, *right, *after_right;

	left = tok;
	op = index_tok(tokens, twoside_find_op(tokens, tok));
	right = index_tok(tokens, twoside_find_op(tokens, tok) + 1);
	after_right = index_tok(tokens, tokens->iter + 2);

	if (!is_operator(op)) {
		error_at(tokens->source, op->value, op->len,
			 "expected operator, got `%.*s`", op->len, op->value);
	}

	if (!is_single_value(tokens, right)) {
		error_at(tokens->source, right->value, op->len,
			 "expected some value, got `%.*s`", right->len,
			 right->value);
	}

	/* we have a two-sided value with an operator */
	node->type = value_expr_type_from_op(op);

	/* if there is a left node defined, it means that is already has been
	   parsed and we only want to parse the right hand side */
	if (!node->left) {
		node->left = calloc(1, sizeof(*node->left));
		if (parse_single_value(context, mod, node->left, tokens,
				       left)) {
			error_at(tokens->source, left->value, op->len,
				 "syntax error when parsing value");
		}
		next_tok(tokens);
	}

	/* right hand side */
	node->right = calloc(1, sizeof(*node->right));
	parse_single_value(context, mod, node->right, tokens, right);
	next_tok(tokens);

	/* resolve the return type of the expression; for now, just
	   assume that it's the same of whateever the return type of the
	   left operand is */
	node->return_type = type_copy(node->left->return_type);

	/* if the is an operator after this expression, set this whole
	   expression to the left-hand side, and pass it again to a
	   parse_twoside */
	if (is_operator(after_right)) {
		value_expr_t *new_node;
		new_node = calloc(1, sizeof(*new_node));
		new_node->left = node;
		node = parse_twoside_value_expr(context, mod, new_node, tokens,
						right);
		next_tok(tokens);
	}

	return node;
}

/**
 * literal   ::= string | integer | float | "null"
 * reference ::= identifier
 * value     ::= literal
 *           ::= reference
 *           ::= call
 *           ::= dereference
 *           ::= pointer-to
 *           ::= value op value
 *           ::= op value
 *
 * value
 */
value_expr_t *parse_value_expr(expr_t *context, expr_t *mod, value_expr_t *node,
			       token_list *tokens, token *tok)
{
	/* literal | reference | call | dereference | pointer-to */
	if (((is_literal(tok) || is_reference(tok) || is_pointer_to(tokens, tok)
	      || is_dereference(tokens, tok))
	     && index_tok(tokens, tokens->iter)->type == T_NEWLINE)
	    || is_call(tokens, tok)) {
		parse_single_value(context, mod, node, tokens, tok);
		return node;
	}

	/* value op value */
	if (is_single_value(tokens, tok)) {
		/* TODO: this operator parser can only do single chain of
		   values, without any parenthesis or operator precendence */
		return parse_twoside_value_expr(context, mod, node, tokens,
						tok);
	}

	error_at(tokens->source, tok->value, tok->len, "failed to parse value");
	return node;
}
