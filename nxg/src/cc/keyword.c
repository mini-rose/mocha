#include <nxg/cc/keyword.h>
#include <stdbool.h>
#include <string.h>

#define LENGTH(array) sizeof(array) / sizeof(*array)

static char *keywords[] = {
    [K_FUNCTION] = "fn", [K_IF] = "if",   [K_ELIF] = "elif",
    [K_ELSE] = "else",   [K_FOR] = "for", [K_WHILE] = "while",
    [K_AND] = "and",     [K_OR] = "or",   [K_NOT] = "not",
    [K_RETURN] = "ret"};

static size_t length = LENGTH(keywords);

bool is_keyword(const char *str)
{
	for (int i = 0; i < length; i++)
		if (!strcmp(str, keywords[i]))
			return true;

	return false;
}

keyword_t get_keyword(const char *str)
{
	for (int i = 0; i < length; i++)
		if (!strcmp(str, keywords[i]))
			return (keyword_t) i;

	return -1;
}
