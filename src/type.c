#include <nxg/type.h>
#include <stdbool.h>
#include <string.h>

#define LENGTH(array) sizeof(array) / sizeof(*array)

static char *types[] = {
    [T_VOID] = "void", [T_BOOL] = "bool", [T_I8] = "i8",     [T_I16] = "i16",
    [T_I32] = "i32",   [T_I64] = "i64",   [T_I128] = "i128", [T_U8] = "u8",
    [T_U16] = "u16",   [T_U32] = "u32",   [T_U64] = "u64",   [T_U128] = "u128",
    [T_F32] = "f32",   [T_F64] = "f64",   [T_STR] = "str",   [T_PTR] = "ptr"};

static size_t length = LENGTH(types);

bool is_type(const char *str)
{
	for (int i = 0; i < length; i++)
		if (!strcmp(str, types[i]))
			return true;

	return false;
}

type_t get_type(const char *str)
{
	for (int i = 0; i < length; i++)
		if (!strcmp(str, types[i]))
			return (type_t) i;

	return -1;
}
