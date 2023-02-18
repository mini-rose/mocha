/* std.builtin.string - string operations
   Copyright (c) 2023 mini-rose */

#include "../coffee.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* drop(&str) -> null */
cf_null _C4dropP3str(cf_str *self)
{
	if (!(self->flags & CF_STR_ALLOC))
		return;

	free(self->ptr);
}

/* copy(&str, &str) -> null */
cf_null _C4copyP3strP3str(cf_str *self, cf_str *from)
{
	if (self->flags & CF_STR_ALLOC)
		_C4dropP3str(self);

	self->flags = CF_STR_ALLOC;
	self->len = from->len;
	self->ptr = (char *) malloc(self->len);
	memcpy(self->ptr, from->ptr, self->len);
}

/* len(&str) -> i64 */
cf_i64 _C3lenP3str(cf_str *self)
{
	return self->len;
}
