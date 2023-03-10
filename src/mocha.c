#include <mocha/mocha.h>
#include <mocha/tools.h>
#include <mocha/utils.h>
#include <stdio.h>
#include <stdlib.h>

static void version()
{
	puts("mocha: " MOCHA_MAJOR "." MOCHA_MINOR "." MOCHA_PATCH "\n"
	     "target: " MOCHA_TARGET);
}

static void help()
{
	puts(
	    "usage: mocha [option]... [action]\n\n"
	    "\033[1;34mActions\033[0m\n"
	    "\tnew <name>         create new project\n"
	    "\tinit               create new project in an existing directory\n"
	    "\tbuild              build project\n"
	    "\trun                run project\n"
	    "\tclean              remove generated artifacts\n\n"
	    "\033[1;34mOptions\033[0m\n"
	    "\t-v, --version      show the version\n"
	    "\t-h, --help         show this page\n"
	    "\t-q, --quiet        silents all unnecessery outputs\n");
}

/* Destroy settings */
static void destroy(settings_t *settings)
{
	if (settings->root)
		free((char *) settings->root);

	if (settings->src)
		free((char *) settings->src);

	if (settings->out)
		free((char *) settings->out);

	if (settings->package_name)
		free((char *) settings->package_name);

	if (settings->package_version)
		free((char *) settings->package_version);
}

/* Call action functions */
static void run_action(settings_t *settings)
{
	switch (settings->action) {
	case A_NEW:
		project_new(settings);
		break;
	case A_RUN:
		break;
	case A_INIT:
		project_init(settings);
		break;
	case A_HELP:
		help();
		break;
	case A_BUILD:
		error("Not implemented yet.");
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
