#include "mocha.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

mo_null __c_printi(mo_i32 num)
{
	printf("%d\n", num);
}

mo_null __c_printl(mo_i64 num)
{
	printf("%ld\n", num);
}

mo_null __c_printa(mo_i8 num)
{
	printf("%hhd\n", num);
}

mo_null __c_printb(mo_bool b)
{
	puts((b) ? "true" : "false");
}

mo_null __c_prints(mo_str *string)
{
	printf("%.*s\n", (int) string->len, string->ptr);
}

mo_null __c_write(mo_i8 *file, mo_i8 *buf, mo_i64 len)
{
	fwrite(buf, 1, len, (FILE *) file);
}

mo_null __c_write_stream(mo_i32 stream, mo_i8 *buf, mo_i64 len)
{
	write(stream, buf, len);
}

mo_i8 *__c_open(mo_str *_path, mo_str *_mode)
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

mo_str *__c_read(mo_i8 *file, mo_i32 n_bytes)
{
	mo_str *self = (mo_str *) malloc(sizeof(mo_str));
	self->ptr = (mo_i8 *) malloc(sizeof(char) * n_bytes);
	self->len = n_bytes;
	fgets(self->ptr, sizeof(char) * n_bytes, (FILE *) file);
	return self;
}

mo_str *__c_readline(mo_i8 *file)
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

	/* Skip new line */
	fgetc((FILE *) file);

	return self;
}

mo_null __c_close(mo_i8 *file)
{
	if (file == NULL)
		return;

	fclose((FILE *) file);
	file = NULL;
}

mo_null __c_rewind(mo_i8 *file)
{
	rewind((FILE *) file);
}

mo_i64 __c_tell(mo_i8 *file)
{
	return ftell((FILE *) file);
}

mo_null __c_seek_set(mo_i8 *file, mo_i64 offset)
{
	fseek((FILE *) file, offset, SEEK_SET);
}

mo_null __c_seek_cur(mo_i8 *file, mo_i64 offset)
{
	fseek((FILE *) file, offset, SEEK_CUR);
}

mo_null __c_seek_end(mo_i8 *file, mo_i64 offset)
{
	fseek((FILE *) file, offset, SEEK_END);
}
