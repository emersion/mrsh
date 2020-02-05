# 2.2.1 Escape Character (Backslash)
# The following must be quoted:
printf '%s\n' \|\&\;\<\>\(\)\$\`\\\"\'
# The following must be quoted only under certain conditions:
printf '%s\n' *?[#˜=%
# Escaping whitespace:
printf 'arg one: %s; arg two: %s\n' with\ space with\	tab
printf 'arg one: %s; arg two: %s\n' without \
	newline
printf 'arg one: %s; arg two: %s\n' without\
whitespace 'arg two'

# 2.2.2 Single quotes
printf '%s\n' '|&;<>()$`\"'

# 2.2.3 Double quotes
printf '%s\n' "|&;<>()'\""

# Parameter & arithmetic expansion should work:
lval=2
rval=3
printf '%s\n' "$lval + ${rval} = $((lval+rval))"
# Command expansion should work
printf '%s\n' "cmd 1: $(echo "cmd 1")"
printf '%s\n' "cmd 2: `echo "cmd 2"`"
# Command expansion should work, recursively
printf '%s\n' "cmd 3: $(echo $(echo "cmd 3"))"

# Backquotes should work
printf '%s\n' "cmd 4: `echo "cmd 4"`"

# Backslashes should escape the following:
printf '%s\n' "\$\`\"\\\
test"
# But not the following:
printf '%s\n' "\|\&\;\<\>\(\)\'"
