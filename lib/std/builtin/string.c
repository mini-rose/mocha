/* std.builtin.string - string operations
   Copyright (c) 2023 mini-rose */

#include "../mocha.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* drop(&str) -> null */
mo_null _M4dropP3str(mo_str *self)
{
	if (!(self->flags & MOCHA_STR_ALLOC))
		return;

	free(self->ptr);
}

/* copy(&str, &str) -> null */
mo_null _M4copyP3strP3str(mo_str *self, mo_str *from)
{
	if (self->flags & MOCHA_STR_ALLOC)
		_M4dropP3str(self);

	self->flags = MOCHA_STR_ALLOC;
	self->len = from->len;
	self->ptr = (char *) malloc(self->len);
	memcpy(self->ptr, from->ptr, self->len);
}

mo_bool _M3cmpP3strP3str(mo_str *string, mo_str *other)
{
	if (string->len != other->len)
		return false;

	return memcmp(string->ptr, other->ptr, string->len) ? false : true;
}

/* len(&str) -> i64 */
mo_i64 _M3lenP3str(mo_str *self)
{
	return self->len;
}

mo_str *_M3stri(mo_i32 num)
{
	char buf[128];
	mo_str *self = (mo_str *) malloc(sizeof(mo_str));

	snprintf(buf, sizeof(buf), "%d", num);

	self->ptr = strdup(buf);
	self->len = strlen(self->ptr);

	return self;
}

mo_str *_M3strl(mo_i64 num)
{
	char buf[128];
	mo_str *self = (mo_str *) malloc(sizeof(mo_str));

	snprintf(buf, sizeof(buf), "%ld", num);

	self->ptr = strdup(buf);
	self->len = strlen(self->ptr);

	return self;
}

mo_str *_M3stra(mo_i8 num)
{
	char buf[128];
	mo_str *self = (mo_str *) malloc(sizeof(mo_str));

	snprintf(buf, sizeof(buf), "%hhd", num);

	self->ptr = strdup(buf);
	self->len = strlen(self->ptr);

	return self;
}

mo_str *_M3strb(mo_bool b)
{
	mo_str *self = (mo_str *) malloc(sizeof(mo_str));

	self->ptr = strdup((b) ? "true" : "false");
	self->len = strlen(self->ptr);

	return self;
}
