#include "mocha.h"

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

mo_i32 _M7executeP3str(mo_str *_cmd)
{
	char __cmd[_cmd->len];
	memcpy(__cmd, _cmd->ptr, _cmd->len);
	__cmd[_cmd->len] = '\0';
	return system(__cmd);
}

mo_str *_M6getenvP3str(mo_str *_name)
{
	mo_str *self;
	char __name[_name->len];
	memcpy(__name, _name->ptr, _name->len);
	__name[_name->len] = '\0';
	self = (mo_str *) malloc(sizeof(mo_str));
	self->ptr = getenv(__name);
	self->len = strlen(self->ptr);
	return self;
}

mo_i32 _M5mkdirP3str(mo_str *_path)
{
	char __path[_path->len];
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';

	if (dir_exists(__path))
		return 1;

	return mkdir(__path, 511);
}

mo_i32 _M5touchP3str(mo_str *_path)
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

mo_i32 _M6renameP3strP3str(mo_str *_old, mo_str *_new)
{
	char __old[_old->len];
	char __new[_new->len];
	memcpy(__old, _old->ptr, _old->len);
	memcpy(__new, _new->ptr, _new->len);
	__old[_old->len] = '\0';
	__new[_new->len] = '\0';
	return rename(__old, __new);
}

mo_i32 _M5chmodP3stri(mo_str *_path, mo_i32 mode)
{
	char __path[_path->len];
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';
	return chmod(__path, mode);
}
