#include "coffee.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

cf_null _C5printi(cf_i32 num)
{
	printf("%d\n", num);
}

cf_null _C5printl(cf_i64 num)
{
	printf("%ld\n", num);
}

cf_null _C5printa(cf_i8 num)
{
	printf("%hhd\n", num);
}

cf_null _C5printb(cf_bool b)
{
	puts((b) ? "true" : "false");
}

cf_null _C5printP3str(cf_str *string)
{
	printf("%.*s\n", (int) string->len, string->ptr);
}

cf_null _C5print3str(cf_str string)
{
	printf("%.*s\n", (int) string.len, string.ptr);
}

cf_null _C5writePaP3str(cf_i8 *file, cf_str *string)
{
	fprintf((FILE *) file, "%.*s", (int) string->len, string->ptr);
}

cf_null _C5writeiP3str(cf_i32 stream, cf_str *string)
{
	switch (stream) {
		case 0:
			fprintf(stdin, "%.*s", (int) string->len, string->ptr);
			break;
		case 1:
			fprintf(stdout, "%.*s", (int) string->len, string->ptr);
			break;
		case 2:
			fprintf(stderr, "%.*s", (int) string->len, string->ptr);
			break;
	}
}

cf_i8 *_C4openP3strP3str(cf_str *path, cf_str *mode)
{
	char *p = strndup(path->ptr, path->len);
	char *m = strndup(mode->ptr, mode->len);

	FILE *fp = fopen(p, m);

	free(p);
	free(m);
	return (cf_i8 *) fp;
}

cf_null _C5closePa(cf_i8 *file)
{
	fclose((FILE *) file);
}
