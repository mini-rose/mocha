#include "coffee.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

cf_null _write(cf_i8 *file, cf_i8 *buf, cf_i64 len)
{
	fwrite(buf, 1, len, (FILE *) file);
}

cf_null _write_stream(cf_i32 stream, cf_i8 *buf, cf_i64 len)
{
	write(stream, buf, len);
}

cf_i8 *_open(cf_str *path, cf_str *mode)
{
	char *p = strndup(path->ptr, path->len);
	char *m = strndup(mode->ptr, mode->len);

	FILE *fp = fopen(p, m);

	free(p);
	free(m);
	return (cf_i8 *) fp;
}

cf_null _close(cf_i8 *file)
{
	fclose((FILE *) file);
}
