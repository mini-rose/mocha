/* std.builtin.string - string operations
   Copyright (c) 2023 mini-rose */

#include "cf_types.h"

#include <stdlib.h>
#include <string.h>

cf_null cf_stralloc(struct cf_str *string, cf_i8 *rawptr, cf_u64 len)
{
	string->ptr = malloc(len);
	memcpy(string->ptr, rawptr, len);
	string->len = len;
}

cf_null cf_strcopy(struct cf_str *source, struct cf_str *dest)
{
	dest->len = source->len;
	dest->ptr = malloc(source->len);
	memcpy(dest->ptr, source->ptr, source->len);
}

cf_null cf_strfree(struct cf_str *string)
{
	free(string->ptr);
}
