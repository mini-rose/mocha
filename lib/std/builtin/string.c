/* std.builtin.string - string operations
   Copyright (c) 2023 mini-rose */

#include "cf_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cf_null cf_strdrop(struct cf_str *string)
{
	if (!string->ref)
		return;

	string->ref--;
	if (!string->ref) {
		free(string->ptr);
	}
}

cf_null cf_strset(struct cf_str *string, cf_i8 *rawptr, cf_i64 len)
{
	if (string->ref)
		cf_strdrop(string);

	string->ptr = malloc(len);
	memcpy(string->ptr, rawptr, len);
	string->len = len;
	string->ref++;
}

cf_null cf_strcopy(struct cf_str *source, struct cf_str *dest)
{
	dest->len = source->len;
	dest->ptr = malloc(source->len);
	memcpy(dest->ptr, source->ptr, source->len);
}

cf_i64 cf_strlen(struct cf_str *string)
{
	return string->len;
}
