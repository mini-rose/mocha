#pragma once

typedef struct {
	char *path;
	char *content;
} file_t;

file_t *file_new(const char *path);
void file_destroy(file_t *f);
