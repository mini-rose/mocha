import std.io

fn main {
	(true) ? {
		io.write('correct\n')
	} : {
		io.write('wrong\n')
	}

	(1 == 4) ? {
		io.write('wrong\n')
	} : {
		io.write('correct\n')
	}
}
