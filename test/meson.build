harness = find_program('./harness.sh')
ref_sh = find_program('sh', required: false)

test_files = [
	'conformance/if.sh',

	'case.sh',
	'loop.sh',
	'subshell.sh',
	'word.sh',
	'for.sh',
	'function.sh',
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
