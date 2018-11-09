project(
	'mrsh',
	'c',
	version: '0.0.0',
	license: 'MIT',
	meson_version: '>=0.43.0',
	default_options: [
		'c_std=c99',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments('-Wno-unused-parameter', language: 'c')
add_project_arguments('-Wno-missing-braces', language: 'c')
add_project_arguments('-Wno-missing-field-initializers', language: 'c')

cc = meson.get_compiler('c')

readline = cc.find_library('readline', required: false)

mrsh_inc = include_directories('include')

install_subdir('include/mrsh', install_dir: get_option('includedir'))

libmrsh_sym_path = meson.current_source_dir() + '/libmrsh.sym'

lib_mrsh = library(
	meson.project_name(),
	files(
		'arithm.c',
		'array.c',
		'ast_print.c',
		'ast.c',
		'buffer.c',
		'builtin/alias.c',
		'builtin/builtin.c',
		'builtin/cd.c',
		'builtin/colon.c',
		'builtin/dot.c',
		'builtin/eval.c',
		'builtin/exit.c',
		'builtin/export.c',
		'builtin/false.c',
		'builtin/getopts.c',
		'builtin/pwd.c',
		'builtin/read.c',
		'builtin/set.c',
		'builtin/shift.c',
		'builtin/times.c',
		'builtin/true.c',
		'builtin/type.c',
		'builtin/unalias.c',
		'builtin/unset.c',
		'hashtable.c',
		'parser/arithm.c',
		'parser/parser.c',
		'parser/program.c',
		'parser/word.c',
		'shell/arithm.c',
		'shell/path.c',
		'shell/process.c',
		'shell/shell.c',
		'shell/task/assignment.c',
		'shell/task/ast.c',
		'shell/task/async.c',
		'shell/task/binop.c',
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
	link_args: ['-Wl,--version-script=' + libmrsh_sym_path],
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
	shell_files += ['readline.c']
else
	shell_files += ['basic.c']
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
