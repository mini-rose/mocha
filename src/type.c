#include <nxg/type.h>

#include <string.h>
#define LEN(x) sizeof(x) / sizeof(x[0])

bool is_type(char *str)
{
	static const char *types[] = {"str", "bool", "i8", "i16", "i32", "i64",
				      "f32", "f64",  "u8", "u16", "u32", "u64"};

	for (int i = 0; i < LEN(types); i++)
		if (!strcmp(str, types[i]))
			return true;

	return false;
}

bool is_kw(char *str)
{
	static const char *keywords[] = {"fn",  "ret",   "if",  "elif", "else",
					 "for", "while", "and", "or",   "not"};

	for (int i = 0; i < LEN(keywords); i++)
		if (!strcmp(str, keywords[i]))
			return true;

	return false;
}
