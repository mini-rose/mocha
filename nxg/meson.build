project(
  'nextgen', 'c', version: '0.1',
  default_options: [
    'prefix=/usr',
    'libdir=lib/coffee',
    'bindir=bin'
  ]
)

sources = [
  'src/nxg.c',
  'src/cc/tokenize.c',
  'src/cc/file.c',
  'src/cc/error.c',
  'src/cc/parser.c',
  'src/cc/parser/is.c',
  'src/cc/parser/free.c',
  'src/cc/parser/value.c',
  'src/cc/parser/call.c',
  'src/cc/parser/dump.c',
  'src/cc/parser/fn.c',
  'src/cc/parser.c',
  'src/cc/type.c',
  'src/cc/keyword.c',
  'src/cc/emit.c',
  'src/cc/module.c',
  'src/cc/mangle.c',
  'src/cc/build.c',
  'src/bs/buildfile.c'
]

includes = include_directories('include')
llvm = declare_dependency(link_args: '-lLLVM')
libdir = get_option('prefix') / get_option('libdir')
bindir = get_option('prefix') / get_option('bindir')

exe = executable(
  'nxg', sources,
  include_directories: includes,
  c_args: ['-O2', '-g', '-DNXG_LIB="' + libdir + '/lib' + '"'],
  link_args: ['-Wall', '-Wextra', '-pedantic'],
  dependencies: llvm,
  install: true,
  install_dir: bindir
)

install_subdir(
  '../lib',
  install_dir : libdir,
  strip_directory : false
)
