/* xc/keyword.c - keyword types
   Copyright (c) 2023 mini-rose */

#include "keyword.h"

#include "utils/error.h"
#include "utils/utils.h"

#include <stdbool.h>
#include <string.h>

static char *keywords[] = {
    [K_FUNCTION] = "fn",   [K_FOR] = "for",       [K_WHILE] = "while",
    [K_AND] = "and",       [K_OR] = "or",         [K_NOT] = "not",
    [K_RETURN] = "return", [K_IMPORT] = "import", [K_TYPE] = "struct",
    [K_IF] = "if",         [K_ELSE] = "else",
};

bool is_keyword(const char *str)
{
	for (size_t i = 0; i < LEN(keywords); i++)
		if (!strcmp(str, keywords[i]))
			return true;

	return false;
}

keyword_t get_keyword(const char *str)
{
	for (size_t i = 0; i < LEN(keywords); i++)
		if (!strcmp(str, keywords[i]))
			return (keyword_t) i;

	error("unknown keyword: '%s'", str);
}
