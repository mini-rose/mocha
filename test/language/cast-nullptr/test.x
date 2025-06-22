import std.io

__builtin_decl("malloc", &null, i64)

fn main {
    x: &i32
    x = malloc(4)
}
