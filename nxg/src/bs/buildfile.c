/* buildfile.c - build system
   Copyright (c) 2023 mini-rose */

#include <ctype.h>
#include <nxg/bs/buildfile.h>
#include <nxg/cc/alloc.h>
#include <nxg/nxg.h>
#include <nxg/utils/error.h>
#include <nxg/utils/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline char *skip_comment(char *p)
{
	while (*(p++) != '\n')
		;

	return ++p;
}

static inline void push_to_settings(file_t *f, settings_t *settings, char *key,
				    int key_len, char *val, int val_len)
{
	if (!strncmp("source", key, key_len)) {
		settings->input = slab_strndup(val, val_len);
	} else if (!strncmp("output", key, key_len)) {
		settings->output = slab_strndup(val, val_len);
	} else if (!strncmp("sysroot", key, key_len)) {
		settings->sysroot = slab_strndup(val, val_len);
	} else {
		error_at(f, key, key_len, "invalid key: '%.*s'", key_len, key);
	}
}

static inline void push_flag_to_settings(file_t *f, settings_t *settings,
					 char *val, int val_len)
{
	if (!strncmp("-p", val, val_len)) {
		settings->show_ast = true;
	} else if (!strncmp("-O", val, val_len - 1)) {
		settings->opt = slab_strndup(val + 2, 1);
	} else if (!strncmp("-L", val, 2)) {
		settings->dyn_linker = slab_strndup(val + 2, val_len - 2);
	} else if (!strncmp("-t", val, val_len)) {
		settings->show_tokens = true;
	} else if (!strncmp("-M", val, val_len)) {
		settings->dyn_linker = slab_strdup("/lib/ld-musl-x86_64.so.1");
	} else if (!strncmp("-V", val, val_len)) {
		settings->verbose = true;
	} else if (!strncmp("-Eno-stack", val, val_len)) {
		settings->emit_stacktrace = false;
	} else if (!strncmp("-Ekeep-var-names", val, val_len)) {
		settings->emit_varnames = true;
	} else {
		error_at(f, val, val_len, "invalid flag: '%.*s'", val_len, val);
	}
}

static inline void parse_list(file_t *f, settings_t *settings, char *key,
			      int key_len, char *p)
{
	if (strncmp("flags", key, key_len))
		error_at(f, key, key_len, "invalid key: '%.*s'", key_len, key);

	while (*p != ']') {
		if (*p == '\'') {
			char *q = strstr(++p, "'");
			push_flag_to_settings(f, settings, p, (int) (q - p));
			p += q - p + 1;
		}

		p++;
	}
}

static inline char *parse_option(file_t *f, settings_t *settings, char *p)
{
	char *q;
	char *key = NULL;
	int key_len = 0;
	char *val = NULL;
	int val_len = 0;

	q = strstr(p, "=");

	key = p;
	key_len = (q++ - p) - 1;

	while (isspace(*q))
		q++;

	if (*q == '\'') {
		char *qq = strstr(++q, "'");
		val = q;
		val_len = qq - q;
		push_to_settings(f, settings, key, key_len, val, val_len);
	} else if (*q == '[') {
		parse_list(f, settings, key, key_len, q);
	}

	while (*p && *p != '\n')
		p++;

	return p;
}

void buildfile(settings_t *settings)
{
	file_t *f = file_new(".mocha");
	char *p = f->content;

	while (*p) {
		if (*p == '#')
			p = skip_comment(p);

		if (isspace(*p))
			p++;

		if (isalpha(*p))
			p = parse_option(f, settings, p);
	}
}
