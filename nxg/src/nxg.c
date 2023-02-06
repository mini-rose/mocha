#include <fcntl.h>
#include <nxg/emit.h>
#include <nxg/file.h>
#include <nxg/module.h>
#include <nxg/nxg.h>
#include <nxg/parser.h>
#include <nxg/tokenize.h>
#include <nxg/type.h>
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
	int opt;

	if (argc == 1) {
		help();
		exit(0);
	}

	for (int i = 0; i < argc; i++) {
		if (!strncmp(argv[i], "--help", 6))
			full_help();
		if (!strncmp(argv[i], "--version", 10))
			version();
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

	settings.input = argv[optind];
	compile(&settings);

	return 0;
}
