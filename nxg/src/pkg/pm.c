/* pkg/pm.c - package manager
   Copyright (c) 2023 mini-rose */

#include "nxg/cc/alloc.h"
#include <limits.h>
#include <nxg/bs/buildfile.h>
#include <nxg/nxg.h>
#include <nxg/pkg/pm.h>
#include <nxg/utils/error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
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

void create_bs_file(const char *dirname, const char *pkgname)
{
	FILE *fp;
	char *bf = "source = 'src/main.ff'\n"
		   "output = 'build/main'\n";
	const char bs_path[PATH_MAX];
	memcpy((void *) bs_path, dirname, strlen(dirname) * sizeof(char) + 1);
	strcat((char *) bs_path, "/.mocha");

	fp = fopen(bs_path, "w");
	fputs(bf, fp);
	fprintf(fp, "project = '%s'\n", pkgname);
	fprintf(fp, "version = 'v0.1.0'\n");
	fclose(fp);
}

void create_main_file(const char *srcdir)
{
	FILE *fp;
	char *content = "fn main {\n\tprint('Hello world!')\n}";
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

	create_bs_file(name, name);
	create_main_file(buf);

	printf("\e[32mCreated\e[0m binary (application) `%s` package\n", name);
}

static void pkghomedir()
{
	char cwd[PATH_MAX];

	while (!file_exists(".mocha")) {
		chdir("..");
		getcwd(cwd, PATH_MAX);

		if (!strcmp(cwd, "/"))
			error("cannot find package home directory.");
	}
}

void pm_build(settings_t *settings)
{
	struct timeval tv;

	pkghomedir();

	if (!dir_exists("build")) {
		mkdir("build", S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH);
	}

	gettimeofday(&tv, NULL);
	settings->pm = true;
	settings->compile_start = (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000LL);

	if (!settings->using_bs) {
		buildfile(settings);
		settings->using_bs = true;
	}

	settings->opt = slab_strdup("0");

	printf("\e[32mCompiling\e[0m %s %s\n", settings->pkgname,
	       settings->pkgver);
}

void pm_run(settings_t *settings)
{
	struct timeval tv;

	pkghomedir();
	pm_build(settings);

	settings->pm_run = true;
	gettimeofday(&tv, NULL);
	settings->compile_start = (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000LL);
}
