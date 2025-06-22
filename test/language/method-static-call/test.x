import std.io

struct Container {
	method: static fn {
		io.write('called method\n')
	}
}

fn main {
	Container.method()
}
