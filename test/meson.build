harness = find_program('./harness.sh')
ref_sh = find_program(get_option('reference-shell'), required: false)

test_files = [
	'args.sh',
	'arithm.sh',
	'async.sh',
	'case.sh',
	'command.sh',
	'for.sh',
	'function.sh',
	'if.sh',
	'loop.sh',
	'pipeline.sh',
	'read.sh',
	'readonly.sh',
	'redir.sh',
	'return.sh',
	'subshell.sh',
	'syntax.sh',
	'ulimit.sh',
	'word.sh',
]

foreach test_file : test_files
	test(
		test_file,
		harness,
		env: [
			'MRSH=@0@'.format(mrsh_exe.full_path()),
			'REF_SH=@0@'.format(ref_sh.path()),
		],
		args: [join_paths(meson.current_source_dir(), test_file)],
	)
endforeach

subdir('conformance')
