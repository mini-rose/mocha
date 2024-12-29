#include "../mocha.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

null _M4dropP3str(str *self)
{
	if (!self->heap)
		return;

	free(self->ptr);
}

null _M4copyP3strP3str(str *self, str *from)
{
    if (!self->heap)
        _M4dropP3str(self);

    self->heap = true;
	self->len = from->len;
	self->ptr = (char *) malloc(self->len);

	memcpy(self->ptr, from->ptr, self->len);
}

i64 _M3lenP3str(str *self)
{
	return self->len;
}
