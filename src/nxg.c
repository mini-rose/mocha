#include <parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline void help()
{
	puts("Usage: nxg [input...] -o [output]");
}

static void compile(const char *path, const char *output)
{
	token_list *list = tokens(path);
	expr *ast = parse(list);

	(void) ast;
}

int main(int argc, char **argv)
{
	char *output = "a.out";
	int opt;

	while ((opt = getopt(argc, argv, "o:h")) != -1) {
		switch (opt) {
			case 'o':
				output = optarg;
				break;
			case 'h':
				help();
		}
	}

	for (int i = optind; i < argc; i++)
		compile(argv[i], output);

	return 0;
}
