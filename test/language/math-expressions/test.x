import std.io

fn main() {
    a: i32 = 10 + 5 // Addition
    b: i32 = 20 - 3 // Subtraction
    c: i32 = 4 * 7 // Multiplication
    d: i32 = 15 / 3 // Division
    e: i32 = 2 % 5 // Modulus

    io.write(str(a))
    io.write('\n')
    io.write(str(b))
    io.write('\n')
    io.write(str(c))
    io.write('\n')
    io.write(str(d))
    io.write('\n')
    io.write(str(e))
    io.write('\n')
}
