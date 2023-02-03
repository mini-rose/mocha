#include <nxg/file.h>
#include <nxg/nxg.h>
#include <nxg/parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline void help()
{
	puts("usage: nxg [-o output] [other options] <input>...\n"
	     "Compile a single or multiple coffee source code files into an "
	     "executable.");
	exit(0);
}

static inline void version()
{
	printf("nxg %d.%d\n", NXG_MAJOR, NXG_MINOR);
	exit(0);
}

static void compile(const char *input, const char *output)
{
	file *source = file_new(input);
	token_list *list = tokens(source);
	expr_t *ast = parse(list);

	expr_print(ast);

	expr_destroy(ast);
	file_destroy(source);
}

int main(int argc, char **argv)
{
	char *output = "a.out";
	int opt;

	if (argc == 1)
		help();

	for (int i = 0; i < argc; i++) {
		if (!strncmp(argv[i], "--help", 6))
			help();
		if (!strncmp(argv[i], "--version", 10))
			version();
	}

	/* NOTE: for our uses, we might want to use a custom argument parser to
	   allow for more complex combinations (along with long options). */

	while ((opt = getopt(argc, argv, "o:hv")) != -1) {
		switch (opt) {
		case 'o':
			output = optarg;
			break;
		case 'h':
			help();
			break;
		case 'v':
			version();
		}
	}

	for (int i = optind; i < argc; i++)
		compile(argv[i], output);

	return 0;
}
