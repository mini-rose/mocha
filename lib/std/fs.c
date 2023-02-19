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


void _C5touchP3stri(cf_str *path, cf_i32 mode)
{
	char *pstr = strndup(path->ptr, path->len);
	FILE *fp = fopen(pstr, "w");
	fclose(fp);
	chmod(pstr, mode);
	free(pstr);
}

cf_bool _C7is_fileP3str(cf_str *path)
{
	struct stat p_stat;
	char *pstr = strndup(path->ptr, path->len);

	if (stat(pstr, &p_stat) != 0)
		return false;

	free(pstr);

	return (S_ISREG(p_stat.st_mode)) ? 1 : 0;
}

cf_bool _C6is_dirP3str(cf_str *path)
{
	struct stat p_stat;
	char *pstr = strndup(path->ptr, path->len);

	if (stat(pstr, &p_stat) != 0)
		return false;

	return S_ISDIR(p_stat.st_mode);
}
