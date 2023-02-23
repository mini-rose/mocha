#include <nxg/cc/alloc.h>
#include <nxg/cc/parser.h>
#include <nxg/cc/tokenize.h>
#include <nxg/cc/type.h>
#include <nxg/utils/error.h>
#include <nxg/utils/utils.h>
#include <stdio.h>
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
	node->name = slab_strndup(tok->value, tok->len);

	var_decl_expr_t *var =
	    node_resolve_local(context, tok->value, tok->len);
	node->return_type = type_copy(var->type);
}

static void parse_member(value_expr_t *node, expr_t *context,
			 token_list *tokens, token *tok)
{
	type_t *temp_type;

	parse_reference(node, context, tokens, tok);
	tok = next_tok(tokens);

	type_t *o_type = node->return_type;

	if (o_type->kind == TY_POINTER)
		o_type = o_type->v_base;

	if (o_type->kind != TY_OBJECT) {
		error_at(tokens->source, tok->value, tok->len,
			 "cannot access members of non-object type");
	}

	tok = next_tok(tokens);

	node->type = VE_MREF;
	node->member = slab_strndup(tok->value, tok->len);

	temp_type = type_object_field_type(o_type->v_object, node->member);
	if (temp_type->kind == TY_NULL) {
		error_at(tokens->source, tok->value, tok->len,
			 "no field named %s found in %s", node->member,
			 o_type->v_object->name);
	}

	node->return_type = temp_type;
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
		node->call = slab_alloc(sizeof(*node->call));
		parse_inline_call(context, mod, node->call, tokens, tok);
		node->return_type = type_copy(node->call->func->return_type);
		return ERR_OK;
	}

	/* member access */
	if (is_member(tokens, tok)) {
		parse_member(node, context, tokens, tok);
		return ERR_OK;
	}

	/* pointer to member */
	if (is_pointer_to_member(tokens, tok)) {
		tok = next_tok(tokens);
		parse_member(node, context, tokens, tok);
		temp_type = node->return_type;
		node->return_type = type_pointer_of(temp_type);
		node->type = VE_MPTR;

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
		return ERR_OK;
	}

	/* pointer-to */
	if (is_pointer_to(tokens, tok)) {
		tok = next_tok(tokens);
		parse_reference(node, context, tokens, tok);
		node->type = VE_PTR;
		temp_type = node->return_type;
		node->return_type = type_pointer_of(node->return_type);
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
		node->left = slab_alloc(sizeof(*node->left));
		if (parse_single_value(context, mod, node->left, tokens,
				       left)) {
			error_at(tokens->source, left->value, op->len,
				 "syntax error when parsing value");
		}
		next_tok(tokens);
	}

	/* right hand side */
	node->right = slab_alloc(sizeof(*node->right));
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
		new_node = slab_alloc(sizeof(*new_node));
		new_node->left = node;
		node = parse_twoside_value_expr(context, mod, new_node, tokens,
						right);
		next_tok(tokens);
	}

	return node;
}

static bool __attribute__((unused)) is_operator_left_to_right(token *op)
{
	struct {
		token_t operator;
		bool right_to_left;
	} operators[] = {{T_EQ, false},  {T_NEQ, false}, {T_ASS, true},
			 {T_ADD, false}, {T_ADDA, true}, {T_DEC, true},
			 {T_INC, true},  {T_SUBA, true}, {T_DIV, false},
			 {T_MOD, false}, {T_DIVA, true}, {T_MODA, true},
			 {T_MUL, false}, {T_SUB, false}};

	for (int i = 0; i < LEN(operators); i++)
		if (operators[i].operator == op->type)
			return operators[i].right_to_left;

	return false;
}

static bool __attribute__((unused))
is_operator_priority(token *op, token *other)
{
	// see more at: https://en.cppreference.com/w/cpp/language/operator_precedence
	struct precedense {
		token_t operator;
		int precedence;
	} priority[] = {{T_EQ, 10},   {T_NEQ, 10}, {T_ASS, 16},  {T_ADD, 6},
			{T_ADDA, 16}, {T_DEC, 3},  {T_INC, 3},   {T_SUBA, 16},
			{T_DIV, 5},   {T_MOD, 5},  {T_DIVA, 16}, {T_MODA, 16},
			{T_MUL, 5},   {T_SUB, 6}};

	struct precedense first;
	struct precedense second;

	for (int i = 0; i < LEN(priority); i++)
		if (priority[i].operator == op->type)
			first = priority[i];

	for (int i = 0; i < LEN(priority); i++)
		if (priority[i].operator == other->type)
			second = priority[i];

	return first.precedence > second.precedence;
}

static value_expr_t *parse_math_value_expr(expr_t *context, expr_t *mod,
					   value_expr_t *node,
					   token_list *tokens, token *tok)
{
	// TODO: parse math
	return parse_twoside_value_expr(context, mod, node, tokens, tok);
}

static value_expr_t *parse_comparison(expr_t *context, expr_t *mod,
				      value_expr_t *node, token_list *tokens,
				      token *tok)
{
	token *left;
	token *right;
	token *op;

	node->left = slab_alloc(sizeof(*node->left));
	node->right = slab_alloc(sizeof(*node->right));
	node->return_type = type_new_plain(PT_BOOL);

	if (!is_single_value(tokens, tok)) {
		error_at(tokens->source, tok->value, tok->len,
			 "left side of comparison is not a valid rvalue");
	}

	left = tok;
	parse_single_value(context, mod, node->left, tokens, tok);

	tok = next_tok(tokens);
	if (tok->type == T_EQ)
		node->type = VE_EQ;
	if (tok->type == T_NEQ)
		node->type = VE_NEQ;

	op = tok;
	tok = next_tok(tokens);

	if (!is_single_value(tokens, tok)) {
		error_at(tokens->source, tok->value, tok->len,
			 "right side of comparison is not a valid rvalue");
	}

	right = tok;
	parse_single_value(context, mod, node->right, tokens, tok);

	if (node->left->return_type->kind != TY_PLAIN) {
		highlight_t hi = highlight_value(tokens, left);
		error_at(tokens->source, hi.value, hi.len,
			 "cannot compare non-integer types");
	}

	if (node->right->return_type->kind != TY_PLAIN) {
		highlight_t hi = highlight_value(tokens, left);
		error_at(tokens->source, hi.value, hi.len,
			 "cannot compare non-integer types");
	}

	return node;
}

/**
 * literal      ::= string | integer | float | "null"
 * reference    ::= ident
 * dereference  ::= *ident
 * pointer-to   ::= &ident
 * member       ::= ident.ident
 * member-deref ::= *ident.ident
 * member-ptr   ::= &ident.ident
 * value        ::= literal
 *              ::= reference
 *              ::= call
 *              ::= dereference
 *              ::= pointer-to
 *              ::= member
 *              ::= member-deref
 *              ::= member-ptr
 *              ::= value <op> value
 *
 * value
 */
value_expr_t *parse_value_expr(expr_t *context, expr_t *mod, value_expr_t *node,
			       token_list *tokens, token *tok)
{
	token *next;

	if (is_comparison(tokens, tok)) {
		return parse_comparison(context, mod, node, tokens, tok);
	}

	next = index_tok(tokens, tokens->iter);
	if (is_operator(next)) {
		return parse_math_value_expr(context, mod, node, tokens,
						tok);
	}

	if (is_single_value(tokens, tok)) {
		if (parse_single_value(context, mod, node, tokens, tok)
		    == ERR_SYNTAX) {
			error_at(tokens->source, tok->value, tok->len,
				 "syntax error, could not parse value");
		}

		return node;
	}

	error_at(tokens->source, tok->value, tok->len, "failed to parse value");
	return node;
}

highlight_t highlight_value(token_list *tokens, token *tok)
{
	highlight_t hi;
	token *tmp;

	hi.value = tok->value;

	if (is_literal(tok)) {
		if (tok->type == T_STRING) {
			hi.len = tok->len + 2;
			hi.value = tok->value - 1;
		} else {
			hi.len = tok->len;
		}
	} else if (is_member(tokens, tok)) {
		tmp = after_tok(tokens, after_tok(tokens, tok));
		hi.len = (size_t) (tmp->value + tmp->len - tok->value);
	} else {
		hi.len = tok->len;
	}

	return hi;
}
