project('nextgen', 'c', version: '0.1')

includes = include_directories('include')

sources = [
  'src/nxg.c',
  'src/tokenize.c',
  'src/file.c',
  'src/error.c',
  'src/parser.c',
  'src/type.c',
  'src/keyword.c',
  'src/emit.c',
  'src/module.c',
  'src/mangle.c'
]

llvm = declare_dependency(link_args: '-lLLVM')

executable(
  'nxg', sources,
  include_directories: includes,
  cpp_args: ['-O0', '-g'],
  link_args: ['-fsanitize=address', '-Wall', '-Wextra', '-pedantic'],
  dependencies: llvm
)
