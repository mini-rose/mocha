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
	if (!string->ref)
		free(string->ptr);
}

/**
 * @function cf_strcopy
 * Copy a new string into this string. If this string already has a value,
 * drop a reference to it first.
 */
cf_null cf_strcopy(struct cf_str *self, struct cf_str *from)
{
	if (self->ref)
		cf_strdrop(self);

	self->ref = 1;
	self->len = from->len;
	self->ptr = malloc(self->len);
	memcpy(self->ptr, from->ptr, self->len);
}

cf_i64 cf_strlen(struct cf_str *self)
{
	return self->len;
}
