harness = find_program('./harness.sh')
ref_sh = find_program('sh', required: false)

test_files = [
	'conformance/if.sh',

	'case.sh',
	'for.sh',
	'function.sh',
	'loop.sh',
	'pipeline.sh',
	'subshell.sh',
	'syntax.sh',
	'ulimit.sh',
	'command.sh',
]

foreach test_file : test_files
	test(
		test_file,
		harness,
		env: [
			'MRSH=@0@'.format(mrsh_exe.full_path()),
			'REF_SH=@0@'.format(ref_sh.path()),
		],
		args: [test_file],
	)
endforeach
