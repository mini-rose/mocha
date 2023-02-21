#include <nxg/cc/alloc.h>
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

	if ((self = (file_t *) slab_alloc(sizeof(file_t))) == NULL)
		error("Cannot allocate memory.");

	if ((input = fopen(path, "r")) == NULL) {
		return NULL;
	}

	fseek(input, 0L, SEEK_END);
	size = ftell(input);
	rewind(input);

	buf = slab_alloc(size + 1);

	if (buf == NULL) {
		fclose(input);
		error("Cannot allocate memory.");
	}

	fread(buf, size, 1, input);

	fclose(input);

	self->path = slab_alloc(strlen(path) * sizeof(char) + 1);
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

file_t *file_stdin()
{
	file_t *self;
	char buf[256];
	int n, amount;

	if ((self = (file_t *) slab_alloc(sizeof(file_t))) == NULL)
		error("failed to slab_alloc mem for file");

	self->path = NULL;
	self->content = NULL;
	amount = 0;

	/* Keep using regular realloc() here, as we don't allocate an array but
	   a contiguous memory block. After collecting everything move it into
	   a controlled slab alloc. */

	while ((fgets(buf, 256, stdin))) {
		n = strlen(buf);
		self->content =
		    realloc(self->content, (amount + n) * sizeof(char));
		memcpy(self->content + amount, buf, n);
		amount += n;
	}

	char *old_content = self->content;

	self->content = slab_alloc(amount + 1);
	memcpy(self->content, old_content, amount);
	self->content[amount] = 0;

	free(old_content);

	return self;
}
