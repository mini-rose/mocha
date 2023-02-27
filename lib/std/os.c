#include "mocha.h"

#include <libgen.h>
#include <limits.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

mo_i32 __c_execute(mo_str *_cmd)
{
	char __cmd[_cmd->len];
	memcpy(__cmd, _cmd->ptr, _cmd->len);
	__cmd[_cmd->len] = '\0';
	return system(__cmd);
}

mo_str *__c_getenv(mo_str *_name)
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

mo_i32 __c_mkdir(mo_str *_path)
{
	char __path[_path->len];
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';

	if (dir_exists(__path))
		return 1;

	return mkdir(__path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH);
}

mo_i32 __c_touch(mo_str *_path)
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

mo_i32 __c_rename(mo_str *_old, mo_str *_new)
{
	char __old[_old->len];
	char __new[_new->len];
	memcpy(__old, _old->ptr, _old->len);
	memcpy(__new, _new->ptr, _new->len);
	__old[_old->len] = '\0';
	__new[_new->len] = '\0';
	return rename(__old, __new);
}

mo_i32 __c_chmod(mo_str *_path, mo_i32 mode)
{
	mode_t _mode = 0;
	char __path[_path->len];
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';
	_mode |= (mode / 100) << 6;
	_mode |= ((mode / 10) % 10) << 3;
	_mode |= mode % 10;
	return chmod(__path, _mode);
}

mo_bool __c_isfile(mo_str *_path)
{
	char __path[_path->len];
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';
	return file_exists(__path);
}

mo_bool __c_isdir(mo_str *_path)
{
	char __path[_path->len];
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';
	return dir_exists(__path);
}

mo_bool __c_islink(mo_str *_path)
{
	struct stat _stat;
	char __path[_path->len];
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';
	return lstat(__path, &_stat) == 0 && S_ISLNK(_stat.st_mode);
}

bool __c_isabs(mo_str *_path)
{
	char __path[_path->len];
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';
	return *__path == '/';
}

mo_str *__c_getcwd()
{
	mo_str *self = (mo_str *) malloc(sizeof(mo_str));
	self->ptr = getcwd(NULL, 0);
	self->len = strlen(self->ptr);
	return self;
}

mo_str *__c_abspath(mo_str *_path)
{
	char __path[_path->len];
	char __abs_path[PATH_MAX];
	mo_str *self = (mo_str *) malloc(sizeof(mo_str));
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';
	getcwd(__abs_path, PATH_MAX);
	strcat(__abs_path, "/");
	strcat(__abs_path, __path);
	self->ptr = strdup(__abs_path);
	self->len = strlen(self->ptr);
	return self;
}

void __c_exit(mo_i32 status)
{
	exit(status);
}

mo_str *__c_basename(mo_str *_path)
{
	mo_str *self = (mo_str *) malloc(sizeof(mo_str));
	char __path[_path->len];
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';
	self->ptr = strdup(basename(__path));
	self->len = strlen(self->ptr);
	return self;
}

mo_str *__c_dirname(mo_str *_path)
{
	mo_str *self = (mo_str *) malloc(sizeof(mo_str));
	char __path[_path->len];
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';
	self->ptr = strdup(dirname(__path));
	self->len = strlen(self->ptr);
	return self;
}

mo_str *__c_expanduser(mo_str *_path)
{
	mo_str *self = (mo_str *) malloc(sizeof(mo_str));
	char __path[_path->len];
	char __expanded[PATH_MAX];
	char *home_path;
	memcpy(__path, _path->ptr, _path->len);
	__path[_path->len] = '\0';
	home_path = getenv("HOME");

	if (!home_path || *__path != '~') {
		self->ptr = strdup("");
		self->len = 0;
	}

	strcat(__expanded, home_path);
	strcat(__expanded, __path + 1);

	puts(__expanded);
	self->ptr = strdup(__expanded);
	self->len = strlen(self->ptr);

	return self;
}
