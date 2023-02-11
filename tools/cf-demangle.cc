/* cf-demangle - demangle Coffee symbols
   Copyright (c) 2023 mini-rose */

#include <cxxabi.h>
#include <stdio.h>

static void demangle(char *sym)
{
	char *res;
	int status;

	sym[1] = 'Z';
	res = abi::__cxa_demangle(sym, NULL, 0, &status);

	if (status) {
		puts("cf-demangle: failed to demangle");
		return;
	}

	puts(res);
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		puts("usage: cf-demangle <symbol>");
		return 0;
	}

	demangle(argv[1]);
}
