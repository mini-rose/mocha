/* print.c - implementation of print routines
   Copyright (c) 2023 mini-rose */

#include "cf_types.h"

#include <stdio.h>

cf_null _C5printi(cf_i32 num)
{
	printf("%d\n", num);
}

cf_null _C5printl(cf_i64 num)
{
	printf("%ld\n", num);
}

cf_null _C5printP3str(struct cf_str *string)
{
	printf("%.*s\n", (int) string->len, string->ptr);
}
