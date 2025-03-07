/* file.h - file operations
   Copyright (c) 2023 mini-rose */

#pragma once

typedef struct {
	char *path;
	char *content;
} file_t;

/* Return NULL instead of throwing an error. */
file_t *file_new_null(const char *path);

file_t *file_new(const char *path);
file_t *file_stdin(void);
