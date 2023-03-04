#include "mocha/mocha.h"
#include <mocha/utils/utils.h>
#include <mocha/tools/argparse.h>
#include <mocha/utils/error.h>
#include <stdio.h>
#include <string.h>

static char *action_name(action_t action)
{
	static char *actions[] = {
		[A_NEW] = "new", [A_RUN] = "run",
		[A_INIT] = "init", [A_HELP] = "help",
		[A_BUILD] = "build", [A_CLEAN] = "clean",
		[A_VERSION] = "version"
	};

	return actions[action];
}

static void parse_option(int argc, char **argv, int index, settings_t *settings)
{
	char *option = argv[index];

	unused(argc);

	if (!strcmp(option, "--help") || !strcmp(option, "-h"))
		settings->action = A_HELP;

	else if (!strcmp(option, "--version") || !strcmp(option, "-v"))
		settings->action = A_VERSION;

	else if (!strcmp(option, "--quiet") || !strcmp(option, "-q"))
		settings->quiet = true;

	else if (!strcmp(option, "-V") || !strcmp(option, "--verbose"))
		settings->verbose = true;

	else
		error("unknown option `%s`", option);
}

static int parse_action(int argc, char **argv, int index, settings_t *settings)
{
	char *action = argv[index];

	if (!strcmp(action, "run"))
		settings->action = A_RUN;

	else if (!strcmp(action, "clean"))
		settings->action = A_CLEAN;

	else if (!strcmp(action, "init"))
		settings->action = A_INIT;

	else if (!strcmp(action, "build")) {
		if (index + 1 < argc) {
			if (!strcmp(argv[index + 1], "release"))
				settings->build_type = B_RELEASE;
			else if (!strcmp(argv[index + 1], "debug"))
				return 1; /* debug is default one */
			else
				error("unknown build type `%s`, allowed only "
				      "`debug` and `release`",
				      argv[index + 1]);

		}

		settings->action = A_BUILD;
	}

	else if (!strcmp(action, "new")) {
		if (index + 1 > argc)
			error("expected package name after 'new'");

		settings->action = A_NEW;
		settings->root = argv[index + 1];
		return 1;
	}

	else
		error("unknown action `%s`", action);

	return 0;
}

static void argparse_verbose(settings_t *settings)
{
	printf("root: %s\n"
	       "outdir: %s\n"
	       "action: %s\n"
	       "build type: %s\n"
	       "opt: %c\n"
	       "verbose: %s\n"
	       "quiet: %s\n",

	       settings->root, settings->outdir, action_name(settings->action),
	       (settings->build_type == B_DEBUG) ? "debug" : "release",
	       settings->opt, (settings->verbose) ? "true" : "false",
	       (settings->quiet) ? "true" : "false");
}

void argparse(int argc, char **argv, settings_t *settings)
{
	/* Skip call argument */
	argc--; argv++;

	for (int i = 0; i < argc; i++) {
		if (*argv[i] == '-')
			parse_option(argc, argv, i, settings);
		else
			i += parse_action(argc, argv, i, settings);
	}

	if (settings->verbose)
		argparse_verbose(settings);
}
