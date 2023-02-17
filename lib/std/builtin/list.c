/* std.builtin.list - list operations
   Copyright (c) 2023 mini-rose */

#include "cf.h"

#include <memory.h>
#include <stdlib.h>

typedef struct
{
	cf_type type;
	union
	{
		cf_i32 v_i32;
		cf_i64 v_i64;
		cf_f32 v_f32;
		cf_bool v_bool;
		struct cf_str v_str;
	};
} cf_object;

typedef struct
{
	cf_object **items;
	cf_i64 len;
	cf_i32 ref;
} cf_list;

cf_null _C4dropP4list(cf_list *self)
{
	if (!self->ref)
		return;

	self->ref--;

	if (!self->ref)
		free(self->items);
}

cf_null _C4copyP4list(cf_list *self, cf_list *from)
{
	if (self->ref)
		_C4dropP4list(self);

	self->ref = 1;
	self->len = from->len;
	self->items = (cf_object **) malloc(sizeof(cf_object) * self->len);
	memcpy(self->items, from->items, sizeof(cf_object) * self->len);
}

cf_i64 _C3lenP4list(cf_list *self)
{
	return self->len;
}

cf_object *_C3getP4listj(cf_list *self, cf_i32 index)
{
	if (index > self->len)
		return NULL;

	return self->items[index];
}

static cf_null append_object(cf_list *self, cf_object *obj)
{
	self->items = (cf_object **) realloc(self->items,
					     sizeof(cf_object) * ++self->len);

	self->items[self->len - 1] = obj;
}

cf_null _C6appendP4listP4list(cf_list *self, cf_list *other)
{
	for (int i = 0; i < other->len; i++)
		append_object(self, other->items[i]);
}

cf_null _C4clearP4list(cf_list *self)
{
	free(self->items);
	self->len = 0;
}

cf_null _C6removeP4listj(cf_list *self, cf_i32 index)
{
	for (int i = index; i < self->len - 1; i++) {
		self->items[i] = self->items[i + 1];
		self->items[i + 1] = NULL;
	}

	self->items = (cf_object **) realloc(self->items,
					     sizeof(cf_object) * --self->len);
}
