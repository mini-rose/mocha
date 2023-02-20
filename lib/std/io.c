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

cf_i8 *_open(cf_str *_path, cf_str *_mode)
{
	char __path[_path->len];
	char __mode[_mode->len];
	memcpy(__path, _path->ptr, _path->len);
	memcpy(__mode, _mode->ptr, _mode->len);
	__path[_path->len] = '\0';
	__mode[_mode->len] = '\0';

	FILE *fp = fopen(__path, __mode);

	return (cf_i8 *) fp;
}

cf_null _close(cf_i8 *file)
{
	if (file == NULL)
		return;

	fclose((FILE *) file);
	file = NULL;
}
