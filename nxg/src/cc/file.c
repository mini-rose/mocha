#include <nxg/utils/error.h>
#include <nxg/utils/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

file_t *file_new_null(const char *path)
{
	char *buf = NULL;
	size_t size;
	file_t *self;
	FILE *input;

	if ((self = (file_t *) malloc(sizeof(file_t))) == NULL)
		error("Cannot allocate memory.");

	if ((input = fopen(path, "r")) == NULL)
		return NULL;

	fseek(input, 0L, SEEK_END);
	size = ftell(input);
	rewind(input);

	buf = calloc(1, size + 1);

	if (buf == NULL) {
		fclose(input);
		error("Cannot allocate memory.");
	}

	fread(buf, size, 1, input);

	fclose(input);

	self->path = calloc(1, strlen(path) * sizeof(char) + 1);
	strcpy(self->path, path);
	self->content = buf;

	return self;
}

file_t *file_new(const char *path)
{
	file_t *f = file_new_null(path);
	if (!f)
		error("Cannot open file - \'%s\'", path);
	return f;
}

void file_destroy(file_t *f)
{
	free(f->path);
	free(f->content);
	free(f);
}
