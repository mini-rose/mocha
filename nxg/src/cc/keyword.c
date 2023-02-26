#include <nxg/cc/keyword.h>
#include <nxg/utils/error.h>
#include <nxg/utils/utils.h>
#include <stdbool.h>
#include <string.h>

static char *keywords[] = {
    [K_FUNCTION] = "fn", [K_FOR] = "for",    [K_WHILE] = "while",
    [K_AND] = "and",     [K_OR] = "or",      [K_NOT] = "not",
    [K_RETURN] = "ret",  [K_IMPORT] = "use", [K_TYPE] = "type"};

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
