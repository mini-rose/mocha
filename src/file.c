#include <error.h>
#include <file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

file *file_new(const char *path)
{
	char *buf = NULL;
	size_t size;
	file *self;
	FILE *input;

	if ((self = (file *) malloc(sizeof(file))) == NULL)
		error("Cannot allocate memory.");

	if ((input = fopen(path, "r")) == NULL)
		error("Cannot open file - \'%s\'", path);

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

void file_destroy(file *f)
{
	free(f->path);
	free(f->content);
	free(f);
}
