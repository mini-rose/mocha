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

int _C6system3str(cf_str cmd)
{
	return _C6systemP3str(&cmd);
}
