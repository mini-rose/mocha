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
	    "  -r, --root <path> coffee root path (default: " NXG_ROOT ")\n"
	    "  -t              show generated tokens\n"
	    "  -v, --version   show the compiler version\n"
	    "  -V              be verbose, show ran shell commands\n"
	    "\nLink:\n"
	    "  -M, --musl      use musl instead of glibc\n"
	    "  -L, --ldd <path> dynamic linker to use (default: " DEFAULT_LD
	    ")\n"
	    "\nEmit:\n"
	    "  -Eno-stack      disable stacktrace\n"
	    "  -Ekeep-var-names keep variable names in LLVM IR\n"
	    "\nRemember that these options may change, so scripts with them "
	    "may not be ideal at this moment.\n",
	    stdout);
	exit(0);
}

static inline void version()
{
	printf("nxg %d.%d\n", NXG_MAJOR, NXG_MINOR);
	printf("target: x86_64\n");
	printf("root: %s\n", NXG_ROOT);
	exit(0);
}

void default_settings(settings_t *settings) {
	settings->sysroot = strdup(NXG_ROOT);
	settings->output = strdup(DEFAULT_OUT);
	settings->global = false;
	settings->input = NULL;
	settings->using_bs = false;
	settings->show_tokens = false;
	settings->dyn_linker = strdup(DEFAULT_LD);
	settings->verbose = false;
	settings->emit_stacktrace = true;
	settings->emit_varnames = false;
	settings->opt = strdup("0");
}

void parse_opt(settings_t *settings, const char *option, char *arg)
{
	if (!strncmp(option, "help", 4))
		full_help();

	if (!strncmp(option, "version", 8))
		version();

	if (!strncmp(option, "musl", 4)) {
		free(settings->dyn_linker);
		settings->dyn_linker = strdup(LD_MUSL);
	}

	if (!strncmp(option, "ldd", 3)) {
		free(settings->dyn_linker);
		settings->dyn_linker = strdup(arg);
	}

	if (!strncmp(option, "root", 4)) {
		free(settings->sysroot);
		settings->sysroot = strdup(arg);
	}
}

void parse_emit_opt(settings_t *settings, const char *option)
{
	if (!strcmp("no-stack", option))
		settings->emit_stacktrace = false;
	else if (!strcmp("keep-var-names", option))
		settings->emit_varnames = true;
	else
		warning("unknown emit option `%s`", option);
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
					   {"ldd", required_argument, 0, 0},
					   {"root", required_argument, 0, 0}};

	while (1) {
		c = getopt_long(argc, argv, "o:r:L:O:E:hvptMV", longopts,
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
		case 'r':
			free(settings.sysroot);
			settings.sysroot = strdup(optarg);
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
		case 'E':
			/* emit options */
			parse_emit_opt(&settings, optarg);
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

	int n = strlen(settings.sysroot);
	if (settings.sysroot[n - 1] == '/')
		settings.sysroot[n - 1] = 0;

	compile(&settings);

	free(settings.dyn_linker);
	free(settings.output);
	free(settings.input);
	free(settings.sysroot);
	free(settings.opt);
	return 0;
}
