#include "coffee.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static inline bool file_exists(char *path)
{
	struct stat _stat;

	if (stat(path, &_stat) != 0)
		return false;

	return (S_ISREG(_stat.st_mode)) ? true : false;
}

static inline bool dir_exists(char *path)
{
	struct stat _stat;

	if (stat(path, &_stat) != 0)
		return false;

	return (S_ISDIR(_stat.st_mode)) ? true : false;
}

cf_i32 _C7executeP3str(cf_str *_cmd)
{
	char __cmd[_cmd->len];
	memcpy(__cmd, _cmd->ptr, _cmd->len);
	__cmd[_cmd->len] = '\0';
	return system(__cmd);
}

cf_str *_C6getenvP3str(cf_str *_name)
{
	cf_str *self;
	char __name[_name->len];
	memcpy(__name, _name->ptr, _name->len);
	__name[_name->len] = '\0';
	self = (cf_str *) malloc(sizeof(cf_str));
	self->ptr = getenv(__name);
	self->len = strlen(self->ptr);
	return self;
}

cf_i32 _C5mkdirP3str(cf_str *_path)
{
	char __path[_path->len];
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';

	if (dir_exists(__path))
		return 1;

	return mkdir(__path, 511);
}

cf_i32 _C5touchP3str(cf_str *_path)
{
	FILE *fp;
	char __path[_path->len];
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';

	if (file_exists(__path))
		return 1;

	fp = fopen(__path, "w");
	fclose(fp);
	return 0;
}

cf_i32 _C6renameP3strP3str(cf_str *_old, cf_str *_new)
{
	char __old[_old->len];
	char __new[_new->len];
	memcpy(__old, _old->ptr, _old->len);
	memcpy(__new, _new->ptr, _new->len);
	__old[_old->len] = '\0';
	__new[_new->len] = '\0';
	return rename(__old, __new);
}

cf_i32 _C5chmodP3stri(cf_str *_path, cf_i32 mode)
{
	char __path[_path->len];
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';
	return chmod(__path, mode);
}
