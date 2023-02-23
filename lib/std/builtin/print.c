#include "../mocha.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

mo_null _M5printi(mo_i32 num)
{
	printf("%d\n", num);
}

mo_null _M5printl(mo_i64 num)
{
	printf("%ld\n", num);
}

mo_null _M5printa(mo_i8 num)
{
	printf("%hhd\n", num);
}

mo_null _M5printb(mo_bool b)
{
	puts((b) ? "true" : "false");
}

mo_null _M5printP3str(mo_str *string)
{
	printf("%.*s\n", (int) string->len, string->ptr);
}

mo_null _M5print3str(mo_str string)
{
	printf("%.*s\n", (int) string.len, string.ptr);
}
