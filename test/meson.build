test_exe = find_program('./test.sh')
ref_exe = find_program('sh', required: false)

test_files = [
	'if.sh',
	'loop.sh',
	'word.sh',
]

foreach test_file : test_files
	test(
		test_file,
		test_exe,
		args: [mrsh_exe.full_path(), ref_exe.path(), 'data/'+test_file],
	)
endforeach
