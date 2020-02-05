harness = find_program('./harness.sh')

test_files = [
	'2.2-quoted-characters.sh',
]

failures = [
	'2.2.2-nested-single-quotes.fail.sh',
	# TODO: https://github.com/emersion/mrsh/issues/145
	#'2.2.3-alias-expansion.fail.sh',
]

undefined = [
	'2.2.3-backquote-nonterminated-squote.undefined.sh',
	'2.2.3-backquote-nonterminated-dquote.undefined.sh',
	'2.2.3-dquote-nonterminated-backquote.undefined.sh',
]

testenv = [
	'MRSH=@0@'.format(mrsh_exe.full_path()),
]

foreach test_file : test_files
	test(
		'conformance/' + test_file,
		harness,
		env: testenv,
		args: [join_paths(meson.current_source_dir(), test_file)],
	)
endforeach

foreach test_file : failures
	test(
		'conformance/' + test_file,
		harness,
		env: testenv,
		args: [join_paths(meson.current_source_dir(), test_file)],
		should_fail: true,
	)
endforeach

if get_option('test-undefined-behavior')
	foreach test_file : undefined
		test(
			'conformance/' + test_file,
			harness,
			env: testenv,
			args: [join_paths(meson.current_source_dir(), test_file)],
			should_fail: true,
		)
	endforeach
endif
