// readline/editline interactive line interface
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_READLINE)
#include <readline/history.h>
#include <readline/readline.h>
#elif defined(HAVE_EDITLINE)
#include <editline/readline.h>
#include <histedit.h>
#endif
#include "frontend.h"

static const char *get_history_path(void) {
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
	if (!rline) {
		return 0;
	}
	size_t len = strlen(rline);
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
