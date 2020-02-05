# 2.2.3 Double quotes
# Should NOT apply alias expansion rules when finding the )
alias myalias="echo )"
var="$(myalias arg-two)"
