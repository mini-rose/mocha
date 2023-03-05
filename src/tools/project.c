/* project.c - basic project actions
   Copyright (c) 2023 mini-rose */

/* @see: mocha/tools/project.h for more information */

#include <libgen.h>
#include <mocha/mocha.h>
#include <mocha/tools/project.h>
#include <mocha/utils/error.h>
#include <mocha/utils/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void project_init(settings_t *settings)
{
	if (!settings->root)
		settings->root = input("Project name: ");

	/* mkdir ./src */
	makedir("src");

	/* creating ./src/main.ff */
	chdir("src");
	FILE *fp = fopen("main.ff", "w");
	fprintf(fp, "fn main {\n\tprint('Hello world!')\n}\n");
	fclose(fp);
	chdir("..");

	/* creating .mocha.cfg */
	fp = fopen(".mocha.cfg", "w");
	fprintf(fp,
		"project = '%s'\nversion = 'v0.1.0'\nsource = "
		"'src/main.ff'\noutput = 'build/main'\n",
		basename((char *) settings->root));
	fclose(fp);

	if (!settings->quiet)
		printf("\033[32mCreated\033[0m `%s` package\n",
		       basename((char *) settings->root));
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

	if (isdir(settings->outdir))
		rmrf(settings->outdir);

	if (isdir("/tmp/mcc"))
		rmrf("/tmp/mcc");
}

void project_build(settings_t *settings)
{
	unused(settings);
	chdir_root();
}

void project_run(settings_t *settings)
{
	unused(settings);
	chdir_root();
}
