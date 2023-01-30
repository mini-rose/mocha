#pragma once

typedef enum
{
	ERR_OK,
	ERR_FAIL
} err_t;

void warning(const char *format, ...);
void error(const char *format, ...);
void error_at(const char *content, const char *pos, const char *format, ...);
