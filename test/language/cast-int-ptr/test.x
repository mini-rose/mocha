use std.io

fn returns_pointer() -> &i32 {
    ret 7
}

fn main {
    a: &i32
    a = returns_pointer()
}
