#include "../x.h"

#include <stdio.h>
#include <stdlib.h>

str *_M3stri(i32 num)
{
	char buf[128];
	str *self = (str *) malloc(sizeof(str));

	snprintf(buf, sizeof(buf), "%d", num);

	self->ptr = strdup(buf);
	self->len = strlen(self->ptr);

	return self;
}

str *_M3strl(i64 num)
{
	char buf[128];
	str *self = (str *) malloc(sizeof(str));

	snprintf(buf, sizeof(buf), "%ld", num);

	self->ptr = strdup(buf);
	self->len = strlen(self->ptr);

	return self;
}

str *_M3stra(i8 num)
{
	char buf[128];
	str *self = (str *) malloc(sizeof(str));

	snprintf(buf, sizeof(buf), "%hhd", num);

	self->ptr = strdup(buf);
	self->len = strlen(self->ptr);

	return self;
}

str *_M3strb(bool b)
{
	str *self = (str *) malloc(sizeof(str));

	self->ptr = strdup((b) ? "true" : "false");
	self->len = strlen(self->ptr);

	return self;
}
