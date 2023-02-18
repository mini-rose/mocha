#include <ctype.h>
#include <nxg/bs/buildfile.h>
#include <nxg/utils/error.h>
#include <nxg/utils/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bs_parse_opt(char *opt_s, char *opt_e, settings_t *settings)
{
	if ((opt_e - opt_s) == 1 && *opt_s != ' ')
		switch (*opt_s) {
			case 'p':
				settings->show_ast = true;
				break;
			case 't':
				settings->show_tokens = true;
				break;
		}
}

void bs_parse_arr(char *key_s, char *key_e, char *val_s, settings_t *settings)
{
	char *p = val_s;

	// options: ['-p', '-v']
	if (strncmp("options", key_s, key_e - key_s))
		error("invalid key: %.*s", key_e - key_s, key_s);

	while (*p != '\n' && *p) {
		if (isspace(*p)) {
			p++;
			continue;
		}

		if (*p == '\'') {
			char *q;

			// Skip string quote and '-'
			p += 2;

			q = strstr(p, "'");

			bs_parse_opt(p, q, settings);
		}

		p++;
	}
}

void bs_parse(char *key_s, char *key_e, char *val_s, settings_t *settings)
{
	char *eol = strstr(val_s, "\n");

	// input: file.ff
	if (!strncmp("input", key_s, key_e - key_s))
		settings->input = strndup(val_s, eol - val_s);

	// output: a.out
	else if (!strncmp("output", key_s, key_e - key_s))
		settings->output = strndup(val_s, eol - val_s);

	else if (!strncmp("stdpath", key_s, key_e - key_s)) {
		free(settings->libpath);
		settings->libpath = strndup(val_s, eol - val_s);
	}

	else
		error("invalid key: %.*s", key_e - key_s, key_s);
}

void buildfile(settings_t *settings)
{
	file_t *f = file_new(".coffee");
	char *p = f->content;
	char *s = p;

	while (*p) {
		if (!strncmp(p, "/*", 2)) {
			char *q = strstr(p + 2, "*/");

			if (!q)
				error_at(f, p, 1, "Unterminated comment.");

			p += q - p + 2;
			continue;
		}

		if (isspace(*p)) {
			p++;
			continue;
		}

		if (*p == ':') {
			char *q = p + 1;

			while (isspace(*q))
				q++;

			if (*q == '[')
				bs_parse_arr(s, p, q + 1, settings);
			else
				bs_parse(s, p, q, settings);

			while (*p != '\n')
				p++;

			s = p + 1;
		}

		p++;
	}

	file_destroy(f);
}
