project(
	'mrsh',
	'c',
	version: '0.0.0',
	license: 'MIT',
	meson_version: '>=0.47.0',
	default_options: [
		'c_std=c99',
		'warning_level=3',
		'werror=true',
	],
)

cc = meson.get_compiler('c')

add_project_arguments(cc.get_supported_arguments([
	'-Wundef',
	'-Wlogical-op',
	'-Wmissing-include-dirs',
	'-Wold-style-definition',
	'-Wpointer-arith',
	'-Winit-self',
	'-Wfloat-equal',
	'-Wstrict-prototypes',
	'-Wredundant-decls',
	'-Wimplicit-fallthrough=2',
	'-Wendif-labels',
	'-Wstrict-aliasing=2',
	'-Woverflow',
	'-Wformat=2',

	'-Wno-missing-braces',
	'-Wno-missing-field-initializers',
	'-Wno-unused-parameter',
]), language: 'c')

if get_option('readline-provider') == 'readline'
	readline = cc.find_library('readline', required: get_option('readline'))
	if readline.found()
		add_project_arguments('-DHAVE_READLINE', language: 'c')
	endif
else # editline
	readline = dependency('libedit', required: get_option('readline'))
	if readline.found()
		add_project_arguments('-DHAVE_EDITLINE', language: 'c')
	endif
endif

mrsh_inc = include_directories('include')

install_subdir('include/mrsh', install_dir: get_option('includedir'))

libmrsh_gnu_sym_path = join_paths(meson.current_source_dir(), 'libmrsh.gnu.sym')
libmrsh_gnu_sym_ldflag = '-Wl,--version-script=' + libmrsh_gnu_sym_path
libmrsh_darwin_sym_path = join_paths(meson.current_source_dir(), 'libmrsh.darwin.sym')
# On FreeBSD, -Wl,--version-script only works with -shared
if cc.links('', name: '-Wl,--version-script', args: ['-shared', libmrsh_gnu_sym_ldflag])
	# GNU ld
	link_args = [libmrsh_gnu_sym_ldflag]
elif host_machine.system() == 'darwin' and cc.has_multi_link_arguments('-Wl,-exported_symbols_list', libmrsh_darwin_sym_path)
	# Clang on Darwin
	link_args = ['-Wl,-exported_symbols_list', libmrsh_darwin_sym_path]
else
	error('Linker doesn\'t support --version-script or -exported_symbols_list')
endif

lib_mrsh = library(
	meson.project_name(),
	files(
		'arithm.c',
		'array.c',
		'ast_print.c',
		'ast.c',
		'buffer.c',
		'builtin/alias.c',
		'builtin/bg.c',
		'builtin/builtin.c',
		'builtin/cd.c',
		'builtin/colon.c',
		'builtin/command.c',
		'builtin/dot.c',
		'builtin/eval.c',
		'builtin/exit.c',
		'builtin/export.c',
		'builtin/false.c',
		'builtin/fg.c',
		'builtin/getopts.c',
		'builtin/pwd.c',
		'builtin/read.c',
		'builtin/set.c',
		'builtin/shift.c',
		'builtin/times.c',
		'builtin/true.c',
		'builtin/type.c',
		'builtin/ulimit.c',
		'builtin/umask.c',
		'builtin/unalias.c',
		'builtin/unset.c',
		'builtin/unspecified.c',
		'builtin/wait.c',
		'getopt.c',
		'hashtable.c',
		'parser/arithm.c',
		'parser/parser.c',
		'parser/program.c',
		'parser/word.c',
		'shell/arithm.c',
		'shell/entry.c',
		'shell/job.c',
		'shell/path.c',
		'shell/process.c',
		'shell/redir.c',
		'shell/shell.c',
		'shell/task/assignment.c',
		'shell/task/ast.c',
		'shell/task/async.c',
		'shell/task/binop.c',
		'shell/task/case_clause.c',
		'shell/task/command_builtin.c',
		'shell/task/command_function.c',
		'shell/task/command_process.c',
		'shell/task/command.c',
		'shell/task/for_clause.c',
		'shell/task/function_definition.c',
		'shell/task/if_clause.c',
		'shell/task/list.c',
		'shell/task/loop_clause.c',
		'shell/task/pipeline.c',
		'shell/task/subshell.c',
		'shell/task/task.c',
		'shell/task/word.c',
		'shell/word.c',
	),
	include_directories: mrsh_inc,
	version: meson.project_version(),
	link_args: link_args,
	install: true,
)

mrsh = declare_dependency(
	link_with: lib_mrsh,
	include_directories: mrsh_inc,
)

shell_deps = [mrsh]
shell_files = [
	'main.c'
]
if readline.found()
	shell_deps += [readline]
	shell_files += ['frontend/readline.c']
else
	shell_files += ['frontend/basic.c']
endif

mrsh_exe = executable(
	'mrsh',
	files(shell_files),
	dependencies: shell_deps,
	install: true,
)

executable(
	'highlight',
	files([
		'highlight.c',
	]),
	dependencies: [mrsh],
)

subdir('test')

pkgconfig = import('pkgconfig')
pkgconfig.generate(
	libraries: lib_mrsh,
	version: meson.project_version(),
	filebase: meson.project_name(),
	name: meson.project_name(),
	description: 'POSIX shell library',
)

status = [
	'',
	'Features:',
	'  readline: @0@'.format(readline.found()),
]
message('\n'.join(status))
