/* std.builtin.string - string operations
   Copyright (c) 2023 mini-rose */

#include "cf_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* drop(&str) -> null */
cf_null _C4dropP3str(struct cf_str *self)
{
	if (!self->ref)
		return;

	self->ref--;

	if (!self->ref)
		free(self->ptr);
}

/* copy(&str, &str) -> null */
cf_null _C4copyP3strP3str(struct cf_str *self, struct cf_str *from)
{
	if (self->ref)
		_C4dropP3str(self);

	self->ref = 1;
	self->len = from->len;
	self->ptr = (char *) malloc(self->len);
	memcpy(self->ptr, from->ptr, self->len);
}

/* len(&str) -> i64 */
cf_i64 _C3lenP3str(struct cf_str *self)
{
	return self->len;
}

/* len(str) -> i64 */
cf_i64 _C3len3str(struct cf_str self)
{
	return self.len;
}
