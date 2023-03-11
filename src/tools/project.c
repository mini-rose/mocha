/* project.c - basic project actions
   Copyright (c) 2023 mini-rose */

/* @see: mocha/tools/project.h for more information */

#include <libgen.h>
#include <mocha/mocha.h>
#include <mocha/tools.h>
#include <mocha/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void project_init(settings_t *settings)
{
	char *name =
	    input("package name: (%s) ", basename((char *) settings->root));

	if (!name)
		settings->package_name =
		    strdup(basename((char *) settings->root));
	else
		settings->package_name = strdup(name);

	char *version = input("Version: (v0.1.0) ");

	/* mkdir ./src */
	makedir("src");

	/* creating ./src/main.ff */
	chdir("src");
	FILE *fp = fopen("main.ff", "w");
	fprintf(fp, "fn main {\n"
		    "\tprint('Hello world!')\n"
		    "}\n");
	fclose(fp);
	chdir("..");

	/* creating .mocha.cfg */
	fp = fopen(".mocha.cfg", "w");
	fprintf(fp,
		"project = '%s'\n"
		"version = '%s'\n"
		"output = 'build/main'\n",
		settings->package_name, (version) ? version : "v0.1.0");
	fclose(fp);

	if (!settings->quiet)
		printf("\033[32mCreated\033[0m `%s` package\n",
		       basename((char *) settings->package_name));
}

void project_new(settings_t *settings)
{
	/* Check if already exists */
	if (isdir(settings->root))
		error("destination `%s` already exists",
		      abspath(settings->root));

	/* mkdir root */
	makedir(settings->root);
	chdir(settings->root);

	/* Initialize project in root path */
	project_init(settings);
}

void project_clean(settings_t *settings)
{
	chdir_root();

	if (isdir(dirname((char *) settings->out)))
		rmrf(settings->out);

	if (isdir("/tmp/mcc"))
		rmrf("/tmp/mcc");
}
