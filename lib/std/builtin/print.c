/* print.c - implementation of print routines
   Copyright (c) 2023 mini-rose */

#include "cf_types.h"

#include <stdio.h>

cf_null cf_print_i32(cf_i32 num)
{
	printf("%d", num);
}
