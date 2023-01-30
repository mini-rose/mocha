#include <nxg/llvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *func_decl(char *name, plain_type ret_type, int nargs, ...)
{
	va_list ap;
	char buf[1024];
	char *ret;
	static const char *types[] = {"",    "str", "i8",  "i16",  "i32",
				      "i64", "f32", "f64", "bool", ""};

	va_start(ap, nargs);

	snprintf(buf, sizeof(buf), "declare %s @%s(", types[ret_type], name);

	for (int i = 0; i < nargs; i++)
		strcat(buf, types[va_arg(ap, int)]);

	va_end(ap);

	ret = calloc(1, strlen(buf) * sizeof(char) + 1);
	strcpy(ret, buf);

	return ret;
}
