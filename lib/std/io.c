#include "mocha.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

mo_null _M5printi(mo_i32 num)
{
	printf("%d\n", num);
}

mo_null _M5printl(mo_i64 num)
{
	printf("%ld\n", num);
}

mo_null _M5printa(mo_i8 num)
{
	printf("%hhd\n", num);
}

mo_null _M5printb(mo_bool b)
{
	puts((b) ? "true" : "false");
}

mo_null _M5printP3str(mo_str *string)
{
	printf("%.*s\n", (int) string->len, string->ptr);
}

mo_null _M5print3str(mo_str string)
{
	printf("%.*s\n", (int) string.len, string.ptr);
}

mo_null _write(mo_i8 *file, mo_i8 *buf, mo_i64 len)
{
	fwrite(buf, 1, len, (FILE *) file);
}

mo_null _write_stream(mo_i32 stream, mo_i8 *buf, mo_i64 len)
{
	write(stream, buf, len);
}

mo_i8 *_open(mo_str *_path, mo_str *_mode)
{
	char __path[_path->len];
	char __mode[_mode->len];
	memcpy(__path, _path->ptr, _path->len);
	memcpy(__mode, _mode->ptr, _mode->len);
	__path[_path->len] = '\0';
	__mode[_mode->len] = '\0';

	FILE *fp = fopen(__path, __mode);

	return (mo_i8 *) fp;
}

mo_str *_read(mo_i8 *file, mo_i32 n_bytes)
{
	mo_str *self = (mo_str *) malloc(sizeof(mo_str));
	self->ptr = (mo_i8 *) malloc(sizeof(char) * n_bytes);
	self->len = n_bytes;
	fgets(self->ptr, sizeof(char) * n_bytes, (FILE *) file);
	return self;
}

mo_str *_readline(mo_i8 *file)
{
	int start_pos;
	int n_bytes;
	mo_str *self = (mo_str *) malloc(sizeof(mo_str));

	start_pos = ftell((FILE *) file);
	n_bytes = 0;

	/* Find line length */
	while (fgetc((FILE *) file) != '\n')
		n_bytes++;

	fseek((FILE *) file, start_pos, SEEK_SET);
	self->ptr = (mo_i8 *) malloc(sizeof(char) * n_bytes);
	self->len = n_bytes;

	for (int i = 0; i < n_bytes; i++)
		self->ptr[i] = fgetc((FILE *) file);

	return self;
}

mo_null _close(mo_i8 *file)
{
	if (file == NULL)
		return;

	fclose((FILE *) file);
	file = NULL;
}
