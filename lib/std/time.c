#include "mocha.h"

#include <memory.h>
#include <time.h>

mo_i64 _M4timev()
{
	return time(NULL);
}

mo_str *_format(mo_i32 time, mo_str *format)
{
	char __format[format->len];
	struct tm* timeinfo;

	timeinfo = localtime((time_t *) &time);
	memcpy(__format, format->ptr, format->len);
	__format[format->len] = '\0';

	strftime(NULL, NULL, __format,
		 )
}
