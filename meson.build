project(
  'mocha', 'c', version: 'v0.1.1',
  default_options: [
    'prefix=/usr',
    'c_std=c2x',
    'bindir=bin'
  ]
)

sources = [
  'src/mocha.c',
  'src/tools/args.c',
  'src/tools/project.c',
  'src/tools/config.c',
  'src/utils/utils.c',
  'src/utils/error.c',
  'src/utils/file.c'
]

includes = include_directories('include')
bindir = get_option('prefix') / get_option('bindir')

c_args = [
  '-Wall', '-Wextra',
  '-DMOCHA_MAJOR=' + '"0"',
  '-DMOCHA_MINOR=' + '"1"',
  '-DMOCHA_PATCH=' + '"1"',
  '-DMOCHA_TARGET=' + '"x86_64-pc-linux-gnu"'
]

link_args = []

if get_option('buildtype') == 'debug'
  c_args += ['-O0', '-ggdb', '-fsanitize=address', '-DDEBUG']
  link_args += '-fsanitize=address'
else
  c_args += '-O3'
endif

exe = executable(
  'mocha', sources,
  include_directories: includes,
  c_args: c_args,
  link_args: link_args,
  install: true,
  install_dir: bindir
)
