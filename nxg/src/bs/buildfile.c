#include <ctype.h>
#include <nxg/bs/buildfile.h>
#include <nxg/utils/error.h>
#include <nxg/utils/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bs_parse_options(char *value, settings_t *settings)
{
	char *p = value;

	while (*p) {
		switch (*p) {
			case 'p':
				settings->show_ast = true;
				break;
		}
	}
}

void bs_parse(char *key_s, char *key_e, char *val_s, settings_t *settings)
{
	char *eol = strstr(val_s, "\n");

	printf("KEY: %.*s\n", (int) (key_e - key_s), key_s);

	if (!strncmp("input", key_s, key_e - key_s)) {
		settings->input = strndup(val_s, eol - val_s);
		printf("VALUE: '%s'\n", settings->input);
		return;
	}

	if (!strncmp("output", key_s, key_e - key_s)) {
		settings->output = strndup(val_s, eol - val_s);
		printf("VALUE: '%s'\n", settings->output);
		return;
	}
}

void buildfile(settings_t *settings)
{
	file *f = file_new(".coffee");
	char *p = f->content;
	char *s = p;

	while (*p) {
		if (isspace(*p)) {
			p++;
			continue;
		}

		if (*p == ':') {
			char *q = p + 1;

			while (isspace(*q))
				q++;

			bs_parse(s, p, q, settings);

			while (*p != '\n')
				p++;

			s = p + 1;
		}

		p++;
	}

	file_destroy(f);

	exit(0);
}
