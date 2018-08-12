project(
	'mrsh',
	'c',
	version: '0.0.0',
	license: 'MIT',
	meson_version: '>=0.43.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments('-Wno-unused-parameter', language: 'c')
add_project_arguments('-Wno-missing-braces', language: 'c')
add_project_arguments('-Wno-missing-field-initializers', language: 'c')

cc = meson.get_compiler('c')

rt = cc.find_library('rt')

mrsh_inc = include_directories('include')

install_subdir('include/mrsh', install_dir: get_option('includedir'))

lib_mrsh = library(
	meson.project_name(),
	files(
		'array.c',
		'ast_print.c',
		'ast.c',
		'buffer.c',
		'builtin/alias.c',
		'builtin/builtin.c',
		'builtin/colon.c',
		'builtin/exit.c',
		'builtin/set.c',
		'builtin/times.c',
		'hashtable.c',
		'parser/parser.c',
		'parser/program.c',
		'parser/word.c',
	),
	include_directories: mrsh_inc,
	version: meson.project_version(),
	install: true,
)

mrsh = declare_dependency(
	link_with: lib_mrsh,
	include_directories: mrsh_inc,
)

executable(
	'mrsh',
	files([
		'main.c',
		'shell/process.c',
		'shell/shell.c',
		'shell/shm.c',
		'shell/task_assignment.c',
		'shell/task_async.c',
		'shell/task_binop.c',
		'shell/task_command.c',
		'shell/task_if_clause.c',
		'shell/task_list.c',
		'shell/task_pipeline.c',
		'shell/task_word.c',
		'shell/task.c',
		'shell/word.c',
	]),
	dependencies: [rt, mrsh],
	install: true,
)

pkgconfig = import('pkgconfig')
pkgconfig.generate(
	libraries: lib_mrsh,
	version: meson.project_version(),
	filebase: meson.project_name(),
	name: meson.project_name(),
	description: 'POSIX shell library',
)
