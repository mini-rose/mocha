/* pkg/pm.c - package manager
   Copyright (c) 2023 mini-rose */

#include <limits.h>
#include <nxg/bs/buildfile.h>
#include <nxg/nxg.h>
#include <nxg/pkg/pm.h>
#include <nxg/utils/error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static inline bool dir_exists(const char *path)
{
	struct stat _stat;

	if (stat(path, &_stat) != 0)
		return false;

	return (S_ISDIR(_stat.st_mode)) ? true : false;
}

static inline bool file_exists(const char *path)
{
	struct stat _stat;

	if (stat(path, &_stat) != 0)
		return false;

	return (S_ISREG(_stat.st_mode)) ? true : false;
}

void create_bs_file(const char *dirname)
{
	FILE *fp;
	char *bf = "source = 'src/main.ff'\n"
		   "output = 'build/main'\n";
	const char bs_path[PATH_MAX];
	memcpy((void *) bs_path, dirname, strlen(dirname) * sizeof(char) + 1);
	strcat((char *) bs_path, "/.mocha");

	fp = fopen(bs_path, "w");
	fputs(bf, fp);
	fclose(fp);
}

void create_main_file(const char *srcdir)
{
	FILE *fp;
	char *content = "use std.io\n\nfn main {\n\tprint('Hello world!')\n}";
	const char main_path[PATH_MAX];
	memcpy((char *) main_path, srcdir, strlen(srcdir) + 1);
	strcat((char *) main_path, "/main.ff");

	fp = fopen(main_path, "w");
	fputs(content, fp);
	fclose(fp);
}

void pm_create_pkg(const char *name)
{
	char buf[PATH_MAX];

	if (dir_exists(name))
		error("directory '%s' already exists.", name);

	memcpy(buf, name, strlen(name) + 1);
	mkdir(name, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH);
	strcat(buf, "/src");
	mkdir(buf, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH);
	create_bs_file(name);
	create_main_file(buf);
}

void pm_build(settings_t *settings)
{
	while (!file_exists(".mocha")) {
		chdir("..");
	}

	mkdir("build", S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH);

	if (!settings->using_bs) {
		buildfile(settings);
		settings->using_bs = true;
	}
}

void pm_run(settings_t *settings)
{
	while (!file_exists(".mocha")) {
		chdir("..");
	}

	mkdir("build", S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH);

	if (!settings->using_bs) {
		buildfile(settings);
		settings->using_bs = true;
		settings->pm_run = true;
	}
}
