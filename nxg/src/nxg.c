/* nxg - coffee compiler, build system & package manager
   Copyright (c) 2023 mini-rose */

#include <nxg/bs/buildfile.h>
#include <nxg/nxg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline void help()
{
	puts("usage: nxg [-o output] [flag]... <input>...");
}

static inline void full_help()
{
	help();
	puts("Compile a single or multiple coffee source code files into an "
	     "executable.\n\n"
	     "  -h, --help      show this page\n"
	     "  -v, --version   show the compiler version\n"
	     "  -o <path>       output binary file name (default: a.out)\n"
	     "  -s <path>       standard library path (default: "
	     "/usr/lib/coffee/std)\n"
	     "  -t              show generated tokens\n"
	     "  -p              show generated AST");
	exit(0);
}

static inline void version()
{
	printf("nxg %d.%d\n", NXG_MAJOR, NXG_MINOR);
	exit(0);
}

int main(int argc, char **argv)
{
	settings_t settings = {0};
	settings.stdpath = strdup("/usr/lib/coffee/std");
	settings.output = strdup("a.out");
	settings.global = false;
	settings.input = NULL;
	settings.using_bs = false;
	settings.show_tokens = false;
	settings.jit = argc == 1;
	int opt;

	/* NOTE: for our uses, we might want to use a custom argument parser to
	   allow for more complex combinations (along with long options). */

	while ((opt = getopt(argc, argv, "o:s:hvpt")) != -1) {
		switch (opt) {
		case 'o':
			settings.output = strdup(optarg);
			break;
		case 's':
			free(settings.stdpath);
			settings.stdpath = strdup(optarg);
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
		case 'v':
			version();
			break;
		}
	}

	for (int i = 0; i < argc; i++) {
		if (!strncmp(argv[i], "--help", 6))
			full_help();
		if (!strncmp(argv[i], "--version", 10))
			version();

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

	free(settings.output);
	free(settings.input);
	free(settings.stdpath);
	return 0;
}
