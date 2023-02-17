/* nxg - coffee compiler, build system & package manager
   Copyright (c) 2023 mini-rose */

#include <getopt.h>
#include <nxg/bs/buildfile.h>
#include <nxg/nxg.h>
#include <nxg/utils/error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline void help()
{
	puts("usage: nxg [-o output] [-h] [option]... <input>");
}

static inline void full_help()
{
	help();
	fputs(
	    "Compile, build and link coffee source code into an executable.\n\n"
	    "Common:\n"
	    "  -h, --help      show this page\n"
	    "  -o <path>       output binary file name (default: " DEFAULT_OUT
	    ")\n"
	    "  -O <level>      optimization level, one of: 0 1 2 3 s\n"
	    "  -p              show generated AST\n"
	    "  -s <path>       standard library path (default: " DEFAULT_STD
	    ")\n"
	    "  -t              show generated tokens\n"
	    "  -v, --version   show the compiler version\n"
	    "  -V              be verbose, show ran shell commands\n"
	    "\nOther:\n"
	    "  -M, --musl      use musl instead of glibc\n"
	    "  -L, --ld <path> dynamic linker to use (default: " DEFAULT_LD
	    ")\n"
	    "\nRemember that these options may change, so scripts with them "
	    "may not be ideal at this moment.\n",
	    stdout);
	exit(0);
}

static inline void version()
{
	printf("nxg %d.%d\n", NXG_MAJOR, NXG_MINOR);
	exit(0);
}

void default_settings(settings_t *settings) {
	settings->stdpath = strdup(DEFAULT_STD);
	settings->output = strdup(DEFAULT_OUT);
	settings->global = false;
	settings->input = NULL;
	settings->using_bs = false;
	settings->show_tokens = false;
	settings->dyn_linker = strdup(DEFAULT_LD);
	settings->verbose = false;
	settings->opt = strdup("0");
}

void parse_opt(settings_t *settings, const char *option, char *arg)
{
	puts(option);

	if (!strncmp(option, "help", 4))
		full_help();

	if (!strncmp(option, "version", 8))
		version();

	if (!strncmp(option, "musl", 4)) {
		free(settings->dyn_linker);
		settings->dyn_linker = strdup(LD_MUSL);
	}

	if (!strncmp(option, "ld", 4)) {
		if (!arg)
			error("missing argument for --ld <path>");
		free(settings->dyn_linker);
		settings->dyn_linker = strdup(arg);
	}
}

int main(int argc, char **argv)
{
	settings_t settings = {0};
	int c, optindx = 0;

	default_settings(&settings);
	settings.jit = argc == 1;

	static struct option longopts[] = {{"help", no_argument, 0, 0},
					   {"version", no_argument, 0, 0},
					   {"musl", no_argument, 0, 0},
					   {"ld", required_argument, 0, 0}};

	while (1) {
		c = getopt_long(argc, argv, "o:s:L:O:hvptMV", longopts,
				&optindx);

		if (c == -1)
			break;

		switch (c) {
		case 0:
			parse_opt(&settings, longopts[optindx].name,
				  optarg);
			break;
		case 'o':
			settings.output = strdup(optarg);
			break;
		case 's':
			free(settings.stdpath);
			settings.stdpath = strdup(optarg);
			break;
		case 'L':
			free(settings.dyn_linker);
			settings.dyn_linker = strdup(optarg);
			break;
		case 'h':
			full_help();
			break;
		case 'p':
			settings.show_ast = true;
			break;
		case 't':
			settings.show_tokens = true;
			break;
		case 'M':
			free(settings.dyn_linker);
			settings.dyn_linker = strdup(LD_MUSL);
			break;
		case 'v':
			version();
			break;
		case 'V':
			settings.verbose = true;
			break;
		case 'O':
			settings.opt = strdup(optarg);
			break;
		}
	}

	/* NOTE: for our uses, we might want to use a custom argument parser to
	   allow for more complex combinations (along with long options). */


	for (int i = 0; i < argc; i++) {
		if (!strncmp(argv[i], ".", 2)) {
			buildfile(&settings);
			settings.using_bs = true;
		}
	}

	if (optind >= argc) {
		fprintf(stderr,
			"\e[91merror\e[0m: missing source file argument\n");
		return 1;
	}

	if (settings.input == NULL)
		settings.input = strdup(argv[optind]);

	int n = strlen(settings.stdpath);
	if (settings.stdpath[n - 1] == '/')
		settings.stdpath[n - 1] = 0;

	compile(&settings);

	free(settings.dyn_linker);
	free(settings.output);
	free(settings.input);
	free(settings.stdpath);
	return 0;
}
