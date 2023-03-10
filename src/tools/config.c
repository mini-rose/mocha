#include "mocha/mocha.h"
#include <ctype.h>
#include <libgen.h>
#include <linux/limits.h>
#include <mocha/tools/config.h>
#include <mocha/utils/error.h>
#include <mocha/utils/file.h>
#include <mocha/utils/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *nextline(const char *p)
{
	while (*p && *p != '\n')
		p++;

	return ++p;
}

static const char *skipspaces(const char *p)
{
	while (*p && isspace(*p))
		p++;

	return p;
}

static void parse_single_value(file_t *file, settings_t *settings, char *k, char *v)
{
	char value[128];
	strncpy(value, v + 1, strlen(v) - 2);

	if (!strcmp(k, "project"))
		settings->package_name = strdup(value);

	else if (!strcmp(k, "version"))
		settings->package_version = strdup(value);

	else if (!strcmp(k, "output"))
		settings->out = strdup(value);

	else
		error_at(file, k, strlen(k), "unknown key");
}

static const char *parse_option(file_t *file, settings_t *settings,
				const char *p)
{
	char k[32];
	char v[128];
	int i;

	// Get key
	for (i = 0; *p; i++) {
		if (*p == '=' || *p == ' ')
			break;

		if (*p)
			k[i] = *p++;
	}

	p = skipspaces(p);

	if (*p != '=')
		error_at(file, p, 1, "expected `=`");
	else
		p++;

	p = skipspaces(p);

	k[i] = '\0';

	for (i = 0; *p; i++) {
		if (*p == '\n')
			break;

		if (*p)
			v[i] = *p++;
	}

	parse_single_value(file, settings, k, v);

	return nextline(p);
}

void cfgparse(settings_t *settings)
{
	/* Skip for actions that does not need config */
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
	const char *p = f->content;

	while (*p) {
		if (*p == '#')
			p = nextline(p);

		if (isspace(*p))
			p = skipspaces(p);

		if (isalpha(*p))
			p = parse_option(f, settings, p);
		else
			break;
	}

	file_destroy(f);
}
