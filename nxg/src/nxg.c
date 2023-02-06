#include <fcntl.h>
#include <jansson.h>
#include <nxg/bs/buildfile.h>
#include <nxg/cc/emit.h>
#include <nxg/cc/module.h>
#include <nxg/cc/parser.h>
#include <nxg/cc/tokenize.h>
#include <nxg/cc/type.h>
#include <nxg/nxg.h>
#include <nxg/utils/error.h>
#include <nxg/utils/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
	     "  -o <path>       output binary file name\n"
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
	settings.output = "a.out";
	settings.global = false;
	settings.input = NULL;
	settings.using_bs = false;

	int opt;

	if (argc == 1) {
		help();
		exit(0);
	}

	/* NOTE: for our uses, we might want to use a custom argument parser to
	   allow for more complex combinations (along with long options). */

	while ((opt = getopt(argc, argv, "o:hvp")) != -1) {
		switch (opt) {
		case 'o':
			settings.output = optarg;
			break;
		case 'h':
			full_help();
			break;
		case 'p':
			settings.show_ast = true;
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

	if (settings.input == NULL)
		settings.input = argv[optind];

	compile(&settings);

	return 0;
}
