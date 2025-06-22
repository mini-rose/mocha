/* std.builtin.string - builtin string calls library
   Copyright (c) 2024-2025 mini-rose */

__builtin_decl_mangled("copy", null, &str, &str)
__builtin_decl_mangled("drop", null, &str)
__builtin_decl_mangled("len", i64, &str)

fn len(s: str) -> i64 {
	return len(&s)
}
