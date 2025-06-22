import std.io

struct Container {
	field1: str
	field2: i32
}

fn drop(self: &Container) {
	io.write('dropped Container\n')
	drop(&self.field1)
}

fn main {
	c: Container
}
