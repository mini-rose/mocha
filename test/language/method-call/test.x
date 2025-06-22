import std.io

struct Container {
	method: fn (self: &Container) {
		io.write('called method\n')
	}
}

fn main {
	c: Container
	c.method()
}
