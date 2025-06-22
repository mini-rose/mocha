import std.io

struct T {
	f: fn (self: &T, string: str) {
		io.write(&string)
	}

	f: fn (self: &T, num: i32) {
		io.write(str(num))
	}

	f: static fn (string: str) {
		io.write(&string)
	}

	f: static fn (num: i32) {
		io.write(str(num))
	}
}

fn main {
	t: T
	t.f('hello')
	t.f(123)
	T.f('where')
	T.f(456)
}
