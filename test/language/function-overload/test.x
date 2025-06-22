import std.io

fn f(string: str) {
	io.write(&string)
}

fn f(num: i32) {
	io.write(str(num))
}

fn main {
	f('hello')
	f(123)
}
