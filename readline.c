/* readline/libedit interactive line interface */
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <mrsh/shell.h>
#include <mrsh/parser.h>
#include "frontend.h"

static const char *get_history_path() {
	static char history_path[PATH_MAX + 1];
	snprintf(history_path, sizeof(history_path),
			"%s/.mrsh_history", getenv("HOME"));
	return history_path;
}

void interactive_init(struct mrsh_state *state) {
	rl_initialize();
	read_history(get_history_path());
}

size_t interactive_next(struct mrsh_state *state,
		char **line, const char *prompt) {
	char *rline = readline(prompt);
	size_t len = 0;
	if (!rline) {
		return 0;
	}
	len = strlen(rline);
	if (!(state->options & MRSH_OPT_NOLOG)) {
		add_history(rline);
		write_history(get_history_path());
	}
	*line = malloc(len + 2);
	strcpy(*line, rline);
	strcat(*line, "\n");
	free(rline);
	return len + 1;
}
