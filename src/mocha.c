#include <mocha/mocha.h>
#include <stdio.h>
#include <stdlib.h>

static void version()
{
	printf("mocha %d.%d\n"
	       "target: %s\n",
	       MOCHA_MAJOR, MOCHA_MINOR, MOCHA_TARGET);
}

static void help()
{
	puts("usage: mocha [option]... [action]\n\n"
	     "\e[1;34mActions\e[0m\n"
	     "\tnew <name>         create new project\n"
	     "\tbuild              build project\n"
	     "\trun                build and run project\n"
	     "\tclean              remove generated artifacts\n"
	     "\e[1;34mOptions\e[0m\n"
	     "\t-v, --version      show the version\n"
	     "\t-h, --help         show this page\n"
	     "\t-V, --verbose      be verbose, show ran shell commands\n"
	     "\t                   and provided options\n"
	     "\t-q, --quiet        silents all unnecessery outputs\n");
}

static void destroy(settings_t *settings)
{
	if (settings->pkgname)
		free((char *) settings->pkgname);
	if (settings->pkgver)
		free((char *) settings->pkgver);
	if (settings->source)
		free((char *) settings->source);
	if (settings->output)
		free((char *) settings->output);
	if (settings->outdir)
		free((char *) settings->outdir);
}

static void run_action(settings_t *settings)
{
	switch (settings->action) {
	case A_NEW:
		project_new(settings);
		break;
	case A_RUN:
		project_run(settings);
		break;
	case A_INIT:
		project_init(settings);
		break;
	case A_HELP:
		help();
		break;
	case A_BUILD:
		project_build(settings);
		break;
	case A_CLEAN:
		project_clean(settings);
		break;
	case A_VERSION:
		version();
		break;
	}

	destroy(settings);
}

int main(int argc, char **argv)
{
	settings_t settings = DEFAULT_SETTINGS;

	/* Parse actions and options */
	argparse(argc, argv, &settings);

	/* Parse .mocha.cfg */
	cfgparse(&settings);

	/* Runs program with parsed option */
	run_action(&settings);

	return 0;
}
