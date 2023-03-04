#include <ctype.h>
#include <linux/limits.h>
#include <mocha/tools/config.h>
#include <mocha/utils/error.h>
#include <mocha/utils/file.h>
#include <mocha/utils/utils.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static inline char *skip_comment(char *p)
{
	while (*(p++) != '\n')
		;

	return ++p;
}

static char *parse_option(file_t *f, settings_t *settings, char *p)
{
	fputc(*p, stdout);

	return p++;
}

void cfgparse(settings_t *settings)
{
	switch (settings->action) {
	case A_NEW:
	case A_INIT:
	case A_HELP:
	case A_VERSION:
		return;
	case A_BUILD:
	case A_CLEAN:
	case A_RUN:
		break;
	}

	chdir_root();

	file_t *f= file_new(".mocha.cfg");
	char *p = f->content;

	while (*p) {
		if (*p == '#')
			p = skip_comment(p);

		if (isspace(*p))
			p++;


		if (isalpha(*p))
			p = parse_option(f, settings, p);
	}

	file_destroy(f);
}
