#include <mocha/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

file_t *file_new(const char *path)
{
	char *buf = NULL;
	size_t size;
	file_t *self;
	FILE *input;

	if ((self = (file_t *) malloc(sizeof(file_t))) == NULL)
		error("Cannot allocate memory.");

	if ((input = fopen(path, "r")) == NULL) {
		return NULL;
	}

	fseek(input, 0L, SEEK_END);
	size = ftell(input);
	rewind(input);

	buf = malloc(size + 1);

	if (buf == NULL) {
		fclose(input);
		error("Cannot allocate memory.");
	}

	fread(buf, size, 1, input);

	fclose(input);

	self->path = strdup(abspath(path));
	self->content = buf;

	return self;
}

void file_destroy(file_t *file)
{
	free((char *) file->path);
	free((char *) file->content);
	free((char *) file);
}
