#include <dirent.h>
#include <linux/limits.h>
#include <mocha/utils/error.h>
#include <mocha/utils/utils.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool isfile(const char *path)
{
	struct stat _stat;

	if (stat(path, &_stat) != 0)
		return false;

	return (S_ISREG(_stat.st_mode)) ? true : false;
}

bool isdir(const char *path)
{
	struct stat _stat;

        if (stat(path, &_stat) != 0)
                return false;

	return (S_ISDIR(_stat.st_mode)) ? true : false;
}

char *abspath(const char *path)
{
	static char abs[PATH_MAX];

	getcwd(abs, PATH_MAX);
	strcat(abs, "/");
	strcat(abs, path);

	return abs;
}

void chdir_root(void)
{
	char cwd[PATH_MAX];
	char start[PATH_MAX];
	getcwd(start, PATH_MAX);

	while (!isfile(".mocha.cfg")) {
		chdir("..");
		getcwd(cwd, PATH_MAX);

		if (!strcmp(cwd, "/"))
			error("could not find `.mocha` in `%s` or any parent directory.", start);
	}
}

void makedir(const char *path)
{
	if (mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH) != 0)
		error("cannot create `%s` directory", abspath(path));
}

char *_buildpath(int argc, ...)
{
	static char path[PATH_MAX];
	va_list ap;
	va_start(ap, argc);

	*path = '\0';

	for (int i = 0; i < argc; i++) {
		strcat(path, va_arg(ap, char *));

		if (i + 1 != argc)
			strcat(path, "/");
	}

	va_end(ap);
	return path;
}

void rmrf(const char *path)
{
	DIR *dir = opendir(path);
	struct dirent *entry;
	char *filepath;

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") != 0
		    && strcmp(entry->d_name, "..") != 0) {
			filepath = buildpath(path, entry->d_name);
			if (entry->d_type == 4) {
				rmrf(filepath);
			} else {
				remove(filepath);
			}
		}
	}

	closedir(dir);
	rmdir(path);
}

char *input(const char *prompt)
{
	static char output[64];
	int c;
	*output = '\0';

	fputs(prompt, stdout);
	while ((c = getc(stdin)) != '\n')
		strcat(output, chartostr(c));

	return output;
}
