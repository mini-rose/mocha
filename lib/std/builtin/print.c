#include "../coffee.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

cf_null _C5printi(cf_i32 num)
{
	printf("%d\n", num);
}

cf_null _C5printl(cf_i64 num)
{
	printf("%ld\n", num);
}

cf_null _C5printa(cf_i8 num)
{
	printf("%hhd\n", num);
}

cf_null _C5printb(cf_bool b)
{
	puts((b) ? "true" : "false");
}

cf_null _C5printP3str(cf_str *string)
{
	printf("%.*s\n", (int) string->len, string->ptr);
}

cf_null _C5print3str(cf_str string)
{
	printf("%.*s\n", (int) string.len, string.ptr);
}
