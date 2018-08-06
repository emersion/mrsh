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

mrsh_inc = include_directories('include')

executable(
	'mrsh',
	files([
		'array.c',
		'ast_print.c',
		'ast.c',
		'buffer.c',
		'builtin/builtin.c',
		'builtin/colon.c',
		'builtin/exit.c',
		'builtin/set.c',
		'builtin/times.c',
		'hashtable.c',
		'main.c',
		'parser.c',
		'shell/process.c',
		'shell/shell.c',
		'shell/task_assignment.c',
		'shell/task_async.c',
		'shell/task_binop.c',
		'shell/task_if_clause.c',
		'shell/task_list.c',
		'shell/task_pipeline.c',
		'shell/task_command.c',
		'shell/task_token.c',
		'shell/task.c',
		'shell/token.c',
	]),
	include_directories: [mrsh_inc],
	install: true,
)
