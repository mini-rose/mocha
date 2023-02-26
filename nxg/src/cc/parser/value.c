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

err_t parse_rvalue(settings_t *settings, expr_t *context, expr_t *mod,
		   value_expr_t *node, token_list *tokens, token *tok)
{
	type_t *temp_type;

	/* '(' rvalue ')' */
	if (tok->type == T_LPAREN) {
		tok = next_tok(tokens);

		parse_value_expr(settings, context, mod, node, tokens, tok);
		node->force_precendence = true;

		tok = next_tok(tokens);
		return ERR_OK;
	}

	/* literal */
	if (is_literal(tok)) {
		parse_literal(node, tokens, tok);
		return ERR_OK;
	}

	/* call */
	if (is_call(tokens, tok)) {
		node->type = VE_CALL;
		node->call = slab_alloc(sizeof(*node->call));
		parse_inline_call(settings, context, mod, node->call, tokens,
				  tok);
		node->return_type = type_copy(node->call->func->return_type);
		return ERR_OK;
	}

	/* member call */
	if (is_member_call(tokens, tok)) {
		node->type = VE_CALL;
		node->call = slab_alloc(sizeof(*node->call));

		node->call->object_name = slab_strndup(tok->value, tok->len);
		tok = next_tok(tokens);
		tok = next_tok(tokens);

		parse_inline_call(settings, context, mod, node->call, tokens,
				  tok);
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
	if (is_symbol(tok)) {
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

	error_at(tokens->source, tok->value, tok->len,
		 "failed to parse single rvalue");
	return ERR_SYNTAX;
}

static bool __attribute__((unused))
operator_predeces(value_expr_type self, value_expr_type other)
{
	// see more at: https://en.cppreference.com/w/cpp/language/operator_precedence
	struct precendence
	{
		value_expr_type op;
		int precedence;
	};

	/* The lower the number is, the "more important" the operation is. */
	static const struct precendence opp[] = {{VE_MUL, 5}, {VE_DIV, 5},
						 {VE_ADD, 6}, {VE_SUB, 6},
						 {VE_EQ, 10}, {VE_NEQ, 10}};

	struct precendence self_p;
	struct precendence other_p;

	for (size_t i = 0; i < LEN(opp); i++) {
		if (opp[i].op == self) {
			self_p = opp[i];
			break;
		}
	}

	for (size_t i = 0; i < LEN(opp); i++) {
		if (opp[i].op == other) {
			other_p = opp[i];
			break;
		}
	}

	return self_p.precedence < other_p.precedence;
}

static inline bool is_twoside_value(value_expr_t *node)
{
	return node->left && node->right;
}

static void swap_evaluation_order(value_expr_t *node)
{
	/*
	 * Before:
	 * node: {left, op, right: {r_left, r_op, r_right}}
	 *
	 * After:
	 * node: {left: {left, op, r_left}, r_op, r_right}
	 */
	value_expr_t *left, *r_left, *r_right;
	value_expr_type op, r_op;

	if (!is_twoside_value(node))
		return;

	left = node->left;
	r_left = node->right->left;
	r_right = node->right->right;

	op = node->type;
	r_op = node->right->type;

	node->left = slab_alloc(sizeof(value_expr_t));
	node->left->left = left;
	node->left->type = op;
	node->left->right = r_left;
	node->left->return_type = type_copy(left->return_type);

	node->type = r_op;
	node->right = r_right;
}

/* Parse a rvalue */
value_expr_t *parse_value_expr(settings_t *settings, expr_t *context,
			       expr_t *mod, value_expr_t *node,
			       token_list *tokens, token *tok)
{
	token *start = tok;
	token *op = tok;

	/* rvalue op rvalue */
	int skip = rvalue_token_len(tokens, tok);
	for (int i = 0; i < skip; i++)
		op = after_tok(tokens, op);

	if (is_operator(op)) {
		node->type = value_expr_type_from_op(tokens, op);
		node->left = slab_alloc(sizeof(*node->left));
		node->right = slab_alloc(sizeof(*node->right));

		parse_rvalue(settings, context, mod, node->left, tokens, tok);

		/* Advance to after the operator */
		while (tok->value != op->value)
			tok = next_tok(tokens);
		tok = next_tok(tokens);

		node->right = parse_value_expr(settings, context, mod,
					       node->right, tokens, tok);

		/*
		 * Now, if we have a higher operator precendence than the next
		 * value expression, swap places with it. This will ensure that
		 * the value for the us will be calculated first, before the
		 * other value. Remember, that if the right hand side value is
		 * enclosed in parenthesis (force_precendence=true), we auto-
		 * -matically have a lower precendence.
		 */

		if (is_twoside_value(node->right)
		    && operator_predeces(node->type, node->right->type)
		    && !node->right->force_precendence) {
			swap_evaluation_order(node);
		}

		tok = index_tok(tokens, tokens->iter - 1);
		if (!type_cmp(node->left->return_type,
			      node->right->return_type)) {
			error_at(tokens->source, start->value,
				 tok->value - start->value,
				 "left and right side of operation are not of "
				 "the same types");
		}

		if (node->type == VE_EQ || node->type == VE_NEQ)
			node->return_type = type_new_plain(PT_BOOL);
		else
			node->return_type = type_copy(node->left->return_type);
		return node;
	}

	/* otherwise, parse a single value */
	if (is_rvalue(tokens, start)) {
		parse_rvalue(settings, context, mod, node, tokens, start);
		return node;
	}

	error_at(tokens->source, start->value, start->len,
		 "failed to parse value");
}

highlight_t highlight_value(token_list *tokens, token *tok)
{
	highlight_t hi;
	token *tmp;
	int skip;

	hi.value = tok->value;

	if (is_literal(tok)) {
		if (tok->type == T_STRING) {
			hi.len = tok->len + 2;
			hi.value = tok->value - 1;
		} else {
			hi.len = tok->len;
		}

	} else if (is_call(tokens, tok) || is_member_call(tokens, tok)) {
		skip = call_token_len(tokens, tok);
		tmp = tok;
		for (int i = 0; i < skip; i++)
			tmp = after_tok(tokens, tmp);
		tmp = before_tok(tokens, tmp);

		hi.len = (size_t) (tmp->value + tmp->len - tok->value);

	} else if (is_dereference(tokens, tok) || is_pointer_to(tokens, tok)) {
		tmp = after_tok(tokens, tok);
		hi.len = (size_t) (tmp->value + tmp->len - tok->value);

	} else if (is_member(tokens, tok)) {
		tmp = after_tok(tokens, after_tok(tokens, tok));
		hi.len = (size_t) (tmp->value + tmp->len - tok->value);

	} else {
		hi.len = tok->len;
	}

	return hi;
}
