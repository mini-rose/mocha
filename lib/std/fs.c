#include "coffee.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void _C5mkdirP3stri(cf_str *path, cf_i32 mode)
{
	char *pstr = strndup(path->ptr, path->len);
	mkdir(pstr, (mode_t) mode);
	free(pstr);
}
