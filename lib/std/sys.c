#include "coffee.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef FILE cf_popen;

int _C6systemP3str(cf_str *cmd)
{
	char *buf;
	int ret;

	buf = strndup(cmd->ptr, cmd->len);
	ret = system(buf);
	free(buf);

	return ret;
}

cf_str *_C6getenvP3str(cf_str *name)
{
	cf_str *self;
	char *buf;

	self = (cf_str *) malloc(sizeof(cf_str));
	buf = strndup(name->ptr, name->len);
	self->ptr = getenv(buf);
	free(buf);
	self->len = strlen(self->ptr);

	return self;
}
