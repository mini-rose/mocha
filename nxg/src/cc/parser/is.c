#include <ctype.h>
#include <nxg/cc/parser.h>
#include <nxg/cc/tokenize.h>
#include <stdio.h>
#include <string.h>

/**
 * name: type
 */
bool is_var_decl(token_list *tokens, token *tok)
{
	if (tok->type != T_IDENT)
		return false;

	tok = after_tok(tokens, tok);
	if (!TOK_IS(tok, T_PUNCT, ":"))
		return false;

	return true;
}

/**
 * [&] ident | datatype
 */
bool is_type(token_list *tokens, token *tok)
{
	if (TOK_IS(tok, T_PUNCT, "&"))
		tok = after_tok(tokens, tok);

	return tok->type == T_IDENT || tok->type == T_DATATYPE;
}

/**
 * ident([arg, ...])
 */
bool is_call(token_list *tokens, token *tok)
{
	if (tok->type != T_IDENT)
		return false;

	tok = after_tok(tokens, tok);
	if (tok->type != T_LPAREN)
		return false;

	/* find the closing brace */
	do {
		tok = after_tok(tokens, tok);
		if (tok->type == T_RPAREN)
			return true;
	} while (tok->type != T_END && tok->type != T_NEWLINE);

	return false;
}

/**
 * ident.ident([arg, ...])
 */
bool is_member_call(token_list *tokens, token *tok)
{
	if (tok->type != T_IDENT)
		return false;

	tok = after_tok(tokens, tok);
	if (tok->type != T_DOT)
		return false;

	tok = after_tok(tokens, tok);
	return is_call(tokens, tok);
}

int call_token_len(token_list *tokens, token *tok)
{
	int len = 2;
	int depth = 0;

	/* skip func name */
	tok = after_tok(tokens, tok);
	if (is_member_call(tokens, tok)) {
		tok = after_tok(tokens, tok);
		tok = after_tok(tokens, tok);
	}

	if (tok->type != T_LPAREN)
		return 0;

	tok = after_tok(tokens, tok);

	depth++;

	while (depth > 0) {
		if (tok->type == T_NEWLINE)
			return 0;

		if (tok->type == T_LPAREN)
			depth++;
		if (tok->type == T_RPAREN)
			depth--;

		len++;
		tok = after_tok(tokens, tok);
	}

	return len;
}

/**
 * literal ::= string | integer | float | "null"
 */
bool is_literal(token *tok)
{
	return tok->type == T_NUMBER || tok->type == T_STRING
	    || tok->type == T_TRUE || tok->type == T_FALSE
	    || TOK_IS(tok, T_DATATYPE, "null");
}

/**
 * symbol ::= ident
 */
bool is_symbol(token *tok)
{
	return tok->type == T_IDENT;
}

/**
 * member ::= ident.ident
 */
bool is_member(token_list *tokens, token *tok)
{
	if (tok->type != T_IDENT)
		return false;

	tok = after_tok(tokens, tok);
	if (tok->type != T_DOT)
		return false;

	tok = after_tok(tokens, tok);
	if (tok->type != T_IDENT)
		return false;

	return true;
}

bool is_member_deref(token_list *tokens, token *tok)
{
	if (tok->type != T_MUL)
		return false;

	tok = after_tok(tokens, tok);
	if (tok->type != T_IDENT)
		return false;

	tok = after_tok(tokens, tok);
	if (tok->type != T_DOT)
		return false;

	tok = after_tok(tokens, tok);
	if (tok->type != T_IDENT)
		return false;

	return true;
}

/**
 * member-ptr ::= &ident.ident
 */
bool is_pointer_to_member(token_list *tokens, token *tok)
{
	if (!TOK_IS(tok, T_PUNCT, "&"))
		return false;

	tok = after_tok(tokens, tok);
	if (tok->type != T_IDENT)
		return false;

	tok = after_tok(tokens, tok);
	if (tok->type != T_DOT)
		return false;

	tok = after_tok(tokens, tok);
	if (tok->type != T_IDENT)
		return false;

	return true;
}

/**
 * dereference ::= *ident
 */
bool is_dereference(token_list *tokens, token *tok)
{
	if (tok->type != T_MUL)
		return false;

	tok = after_tok(tokens, tok);
	if (tok->type == T_IDENT)
		return true;

	return false;
}

/**
 * pointer-to ::= &ident
 */
bool is_pointer_to(token_list *tokens, token *tok)
{
	if (!TOK_IS(tok, T_PUNCT, "&"))
		return false;

	tok = after_tok(tokens, tok);
	if (tok->type == T_IDENT)
		return true;

	return false;
}

/*
 * rvalue ::= '(' rvalue ')'
 *        ::= rvalue op rvalue
 *        ::= literal
 *        ::= ident
 *        ::= dereference
 *        ::= pointer-to
 *        ::= member
 *        ::= member-deref
 *        ::= member-pointer-to
 *        ::= call
 *        ::= member-call
 */
bool is_rvalue(token_list *tokens, token *tok)
{
	/* '(' rvalue ')' */
	if (tok->type == T_LPAREN) {
		int offset = rvalue_token_len(tokens, tok) - 1;
		for (int i = 0; i < offset; i++)
			tok = after_tok(tokens, tok);

		if (tok->type != T_RPAREN)
			return false;
		return true;
	}

	/* single rvalue */
	if (is_literal(tok) || is_symbol(tok) || is_call(tokens, tok)
	    || is_dereference(tokens, tok) || is_pointer_to(tokens, tok)
	    || is_member(tokens, tok) || is_member_deref(tokens, tok)
	    || is_pointer_to_member(tokens, tok)
	    || is_member_call(tokens, tok)) {
		return true;
	}

	/* If this is an rvalue, the previous if block will accept any rvalue
	   type, meaning that checking if we have an operator after this is
	   unnecessary, as we would return true even if there wasn't any
	   operator. */

	return false;
}

bool is_operator(token *tok)
{
	/* TODO: make this better and safer */
	return tok->type >= T_EQ && tok->type <= T_SUB;
}

/*
 * comparison ::= rvalue == rvalue
 *            ::= rvalue != rvalue
 */
bool is_comparison(token_list *tokens, token *tok)
{
	int offset = 0;

	if (is_call(tokens, tok)) {
		offset = call_token_len(tokens, tok);
	}

	offset = rvalue_token_len(tokens, tok);
	for (int i = 0; i < offset; i++)
		tok = after_tok(tokens, tok);

	if (tok->type == T_EQ || tok->type == T_NEQ)
		return true;

	return false;
}

bool is_integer(token *tok)
{
	for (int i = 0; i < tok->len; i++) {
		if (!isdigit(tok->value[i]))
			return false;
	}

	return true;
}

bool is_float(token *tok)
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

/**
 * [*]name[.field][: type] = value
 */
bool is_var_assign(token_list *tokens, token *tok)
{
	/* [*] */
	if (tok->type == T_MUL)
		tok = after_tok(tokens, tok);

	if (tok->type != T_IDENT)
		return false;

	tok = after_tok(tokens, tok);

	/* [.member] */
	if (tok->type == T_DOT) {
		tok = after_tok(tokens, tok);
		if (tok->type != T_IDENT)
			return false;

		tok = after_tok(tokens, tok);
	}

	/* [: type] */
	if (TOK_IS(tok, T_PUNCT, ":")) {
		tok = after_tok(tokens, tok);
		if (!is_type(tokens, tok))
			return false;

		tok = after_tok(tokens, tok);
	}

	if (tok->type != T_ASS)
		return false;

	return true;
}

int rvalue_token_len(token_list *tokens, token *tok)
{
	/* Note: keep these if statements from the highest toklen to the
	   smallest, so it can be properly checked. */

	/* '(' rvalue ')' */
	if (tok->type == T_LPAREN) {
		int offset = 0;
		int depth = 0;

		while (1) {
			if (tok->type == T_LPAREN)
				depth++;
			if (tok->type == T_RPAREN)
				depth--;
			if (!depth)
				break;

			tok = after_tok(tokens, tok);
			offset++;
		}

		return offset + 1;
	}

	if (is_call(tokens, tok) || is_member_call(tokens, tok))
		return call_token_len(tokens, tok);
	if (is_member_deref(tokens, tok) || is_pointer_to_member(tokens, tok))
		return 4;
	if (is_member(tokens, tok))
		return 3;
	if (is_dereference(tokens, tok) || is_pointer_to(tokens, tok))
		return 2;
	if (is_literal(tok) || is_symbol(tok))
		return 1;

	return 1;
}
