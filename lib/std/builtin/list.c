/* std.builtin.list - list operations
   Copyright (c) 2023 mini-rose */

#include "../mocha.h"

#include <memory.h>
#include <stdlib.h>

typedef struct
{
	mo_type type;
	union
	{
		mo_i32 v_i32;
		mo_i64 v_i64;
		mo_f32 v_f32;
		mo_bool v_bool;
		mo_str v_str;
	};
} mo_object;

typedef struct
{
	mo_object **items;
	mo_i64 len;
	mo_i32 ref;
} mo_list;

mo_null _M4dropP4list(mo_list *self)
{
	if (!self->ref)
		return;

	self->ref--;

	if (!self->ref)
		free(self->items);
}

mo_null _M4copyP4list(mo_list *self, mo_list *from)
{
	if (self->ref)
		_M4dropP4list(self);

	self->ref = 1;
	self->len = from->len;
	self->items = (mo_object **) malloc(sizeof(mo_object) * self->len);
	memcpy(self->items, from->items, sizeof(mo_object) * self->len);
}

mo_i64 _M3lenP4list(mo_list *self)
{
	return self->len;
}

mo_object *_M3getP4listj(mo_list *self, mo_i32 index)
{
	if (index > self->len)
		return NULL;

	return self->items[index];
}

static mo_null append_object(mo_list *self, mo_object *obj)
{
	self->items = (mo_object **) realloc(self->items,
					     sizeof(mo_object) * ++self->len);

	self->items[self->len - 1] = obj;
}

mo_null _M6appendP4listP4list(mo_list *self, mo_list *other)
{
	for (int i = 0; i < other->len; i++)
		append_object(self, other->items[i]);
}

mo_null _M4clearP4list(mo_list *self)
{
	free(self->items);
	self->len = 0;
}

mo_null _M6removeP4listj(mo_list *self, mo_i32 index)
{
	for (int i = index; i < self->len - 1; i++) {
		self->items[i] = self->items[i + 1];
		self->items[i + 1] = NULL;
	}

	self->items = (mo_object **) realloc(self->items,
					     sizeof(mo_object) * --self->len);
}
