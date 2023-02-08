/* cf-demangle - demangle Coffee symbols
   Copyright (c) 2023 mini-rose */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *mangled_char_type(char c)
{
	/* nxg/src/cc/mangle.c */
	static const char *type_mangle_ids[] = {
	    ['v'] = "null", ['b'] = "bool", ['a'] = "i8",   ['h'] = "u8",
	    ['s'] = "i16",  ['t'] = "u16",  ['i'] = "i32",  ['j'] = "u32",
	    ['l'] = "i64",  ['m'] = "u64",  ['n'] = "i128", ['o'] = "u128",
	    ['f'] = "f32",  ['d'] = "f64"};
	static const int n = sizeof(type_mangle_ids) / sizeof(*type_mangle_ids);

	if (c < n)
		return type_mangle_ids[c];
	return NULL;
}

static void demangle(char *sym)
{
	int len, n;

	sym += 2; /* _C */

	len = strtol(sym, &sym, 10);
	printf("%.*s(", len, sym);
	sym += len;

	while (*sym) {
		/* &T */
		if (*sym == 'P') {
			fputc('&', stdout);
			sym++;
		}

		/* T[n] */
		if (*sym == 'A') {
			fprintf(stderr, "error: arrays are not impl yet\n");
			return;
		}

		/* object */
		if (isdigit(*sym)) {
			len = strtol(sym, &sym, 10);
			printf("%.*s, ", len, sym);
			sym += len;
		} else {
			printf("%s, ", mangled_char_type(*(sym++)));
		}
	}

	puts(")");
	return;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		puts("usage: cf-demangle <symbol>");
		return 0;
	}

	demangle(argv[1]);
}
