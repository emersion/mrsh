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
		'main.c',
		'parser.c',
		'shell.c',
	]),
	include_directories: [mrsh_inc],
	install: true,
)
