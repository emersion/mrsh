#include <stdlib.h>

#include "ast.h"
#include "parser.h"

int main(int argc, char *argv[]) {
	struct mrsh_program *prog = mrsh_parse(stdin);
	mrsh_program_print(prog);
	return EXIT_SUCCESS;
}
