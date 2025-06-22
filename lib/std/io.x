__builtin_decl("__io__read__", null, i32, &str, i64)
__builtin_decl("__io__write__", i64, i32, &str, i64)
__builtin_decl("__io__popen__", &str, &str, &str)
__builtin_decl("__io__flush__", null, i32)

struct io {
	write: static fn (buf: &str) -> i64 {
		return __io__write__(1, buf, len(buf))
	}

	write: static fn (fd: i32, buf: &str) -> i64 {
		return __io__write__(fd, buf, len(buf))
	}

	write: static fn (fd: i32, buf: &str, count: i64) -> i64 {
		return __io__write__(fd, buf, count)
	}

	write: static fn (buf: str) -> i64 {
		return __io__write__(1, &buf, len(buf))
	}

	write: static fn (fd: i32, buf: str) -> i64 {
		return __io__write__(fd, &buf, len(buf))
	}

	write: static fn (fd: i32, buf: str, count: i64) -> i64 {
		return __io__write__(fd, &buf, count)
	}

	read: static fn (buf: &str) {
		__io__read__(0, buf, -1)
	}

	read: static fn (buf: &str, count: i64) {
		__io__read__(0, buf, count)
	}

	read: static fn (fd: i32, buf: &str, count: i64) {
		__io__read__(fd, buf, count)
	}

	flush: static fn (fd: i32) {
		__io__flush__(fd)
	}

	popen: static fn (prog: str) -> &str {
		mode: str = "r"
		return __io__popen__(&prog, &mode)
	}
}
