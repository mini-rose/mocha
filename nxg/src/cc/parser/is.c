#include "nxg/cc/tokenize.h"

#include <ctype.h>
#include <nxg/cc/parser.h>
#include <stdio.h>
#include <string.h>

/**
 * name: type
 */
bool is_var_decl(token_list *tokens, token *tok)
{
	if (tok->type != T_IDENT)
		return false;

	tok = index_tok(tokens, tokens->iter);
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
		tok = index_tok(tokens, tokens->iter);

	return tok->type == T_IDENT || tok->type == T_DATATYPE;
}

/**
 * ident([arg, ...])
 */
bool is_call(token_list *tokens, token *tok)
{
	int index = tokens->iter;

	if (tok->type != T_IDENT)
		return false;

	tok = index_tok(tokens, index);
	if (tok->type != T_LPAREN)
		return false;

	/* find the closing brace */
	do {
		tok = index_tok(tokens, ++index);
		if (tok->type == T_RPAREN)
			return true;
	} while (tok->type != T_END && tok->type != T_NEWLINE);

	return false;
}

int call_token_len(token_list *tokens, token *tok)
{
	int len = 2;
	int depth = 0;

	tok = index_tok(tokens, tokens->iter);

	if (tok->type != T_LPAREN)
		return 0;

	depth++;

	while (depth > 0) {
		tok = index_tok(tokens, tokens->iter + len);
		if (tok->type == T_NEWLINE)
			return 0;

		if (tok->type == T_LPAREN)
			depth++;
		if (tok->type == T_RPAREN)
			depth--;

		len++;
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
 * reference ::= ident
 */
bool is_reference(token *tok)
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
 * value ::= literal
 *       ::= ident
 *       ::= call
 *       ::= dereference
 *       ::= pointer-to
 */
bool is_single_value(token_list *tokens, token *tok)
{
	return is_literal(tok) || is_reference(tok) || is_call(tokens, tok)
	    || is_dereference(tokens, tok) || is_pointer_to(tokens, tok);
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

	if (is_pointer_to(tokens, tok)) {
		offset = 1;
	} else if (is_member(tokens, tok)) {
		offset = 2;
	} else if (is_dereference(tokens, tok)) {
		offset = 1;
	} else if (is_pointer_to_member(tokens, tok)) {
		offset = 3;
	} else if (is_member_deref(tokens, tok)) {
		offset = 3;
	}

	tok = index_tok(tokens, tokens->iter + offset);
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
 * name[.field][: type] = value
 */
bool is_var_assign(token_list *tokens, token *tok)
{
	int offset = 0;

	if (tok->type == T_MUL) {
		tok = after_tok(tokens, tok);
		offset++;
	}

	if (tok->type != T_IDENT)
		return false;

	tok = index_tok(tokens, tokens->iter + offset);
	if (tok->type == T_DOT)
		offset++;

	tok = index_tok(tokens, tokens->iter + offset);
	if (tok->type == T_IDENT)
		offset++;

	tok = index_tok(tokens, tokens->iter + offset);
	if (TOK_IS(tok, T_PUNCT, ":")) {
		offset++;
		tok = index_tok(tokens, tokens->iter + offset);
		if (tok->type != T_DATATYPE && tok->type != T_IDENT)
			return false;
		offset++;
	}

	tok = index_tok(tokens, tokens->iter + offset);
	if (tok->type != T_ASS)
		return false;

	return true;
}
