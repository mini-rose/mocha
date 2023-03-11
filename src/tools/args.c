#include <mocha/mocha.h>
#include <mocha/tools.h>
#include <mocha/utils.h>
#include <stdio.h>
#include <string.h>

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

	else if (!strcmp(action, "new")) {
		if (index + 1 >= argc)
			error("expected package name after 'new'");

		settings->action = A_NEW;
		settings->root = strdup(argv[index + 1]);
		return 1;
	}

	else
		error("unknown action `%s`", action);

	return 0;
}

#ifdef DEBUG
char *action_name(action_t action)
{
	static char *names[] = {
	    [A_RUN] = "run", [A_CLEAN] = "clean", [A_INIT] = "init",
	    [A_NEW] = "new", [A_HELP] = "help",   [A_VERSION] = "version"};

	return names[action];
}
#endif

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

	debug("action: %s", action_name(settings->action));
}
