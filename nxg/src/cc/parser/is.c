#include "nxg/cc/tokenize.h"

#include <ctype.h>
#include <nxg/cc/parser.h>
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

	tok = index_tok(tokens, tokens->iter + 1);
	if (!is_type(tokens, tok))
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
	if (!TOK_IS(tok, T_PUNCT, "("))
		return false;

	/* find the closing brace */
	do {
		tok = index_tok(tokens, ++index);
		if (TOK_IS(tok, T_PUNCT, ")"))
			return true;
	} while (tok->type != T_END && tok->type != T_NEWLINE);

	return false;
}

/**
 * literal ::= string | integer | float | "null"
 */
bool is_literal(token *tok)
{
	return tok->type == T_NUMBER || tok->type == T_STRING
	    || TOK_IS(tok, T_DATATYPE, "null");
}

bool is_reference(token *tok)
{
	return tok->type == T_IDENT;
}

/**
 * dereference ::= *ident
 */
bool is_dereference(token_list *tokens, token *tok)
{
	if (!TOK_IS(tok, T_OPERATOR, "*") && !TOK_IS(tok, T_PUNCT, "*"))
		return false;

	tok = index_tok(tokens, tokens->iter);
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

	tok = index_tok(tokens, tokens->iter);
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
