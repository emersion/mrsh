// readline/editline interactive line interface
#define _POSIX_C_SOURCE 200809L
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(HAVE_READLINE)
#include <readline/history.h>
#include <readline/readline.h>
#elif defined(HAVE_EDITLINE)
#include <editline/readline.h>
#include <histedit.h>
#endif
#include "frontend.h"

#if defined(HAVE_READLINE)
#if !defined(HAVE_READLINE_REPLACE_LINE)
static void rl_replace_line(const char *text,
                            int clear_undo) {
    return;
}
#endif

static void sigint_handler(int n) {
	/* Signal safety is done here on a best-effort basis. rl_redisplay is not
	 * signal safe, but under these circumstances it's very likely that the
	 * interrupted function will not be affected. */
	char newline = '\n';
	(void)write(STDOUT_FILENO, &newline, 1);
	rl_on_new_line();
	rl_replace_line("", 0);
	rl_redisplay();
}
#endif

static int mkdir_all(const char *path) {
	struct stat st = {0};
	if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
		return 0;
	}

	char *subpath = strdup(path);
	subpath = dirname(subpath);

	if (strcmp(subpath, ".") != 0 && strcmp(subpath, "/") != 0) {
		int ret = mkdir_all(subpath);
		if (ret != 0) {
			return ret;
		}
	}

	free(subpath);

	return mkdir(path, 0700);
}

static char *get_history_path(void) {
	int len = strlen("/history");
	const char *xdg_data_home = getenv("XDG_DATA_HOME");
	const char *home = getenv("HOME");
	if (xdg_data_home) {
		len += snprintf(NULL, 0, "%s/mrsh", xdg_data_home);
	} else {
		len += snprintf(NULL, 0, "%s/.local/share/mrsh", home);
	}

	char *path = malloc(len + 1);
	if (path == NULL) {
		return NULL;
	}

	if (xdg_data_home) {
		len += snprintf(path, len, "%s/mrsh", xdg_data_home);
	} else {
		len += snprintf(path, len, "%s/.local/share/mrsh", home);
	}

	mkdir_all(path);

	strncat(path, "/history", len - strlen(path) - 1);
	return path;
}

void interactive_init(struct mrsh_state *state) {
	rl_initialize();
	char *history_path = get_history_path();
	read_history(history_path);
	free(history_path);
}

size_t interactive_next(struct mrsh_state *state,
		char **line, const char *prompt) {
	/* TODO: make SIGINT handling work with editline */
#if defined(HAVE_READLINE)
	struct sigaction sa = { .sa_handler = sigint_handler }, old;
	sigaction(SIGINT, &sa, &old);
#endif
	char *rline = readline(prompt);
#if defined(HAVE_READLINE)
	sigaction(SIGINT, &old, NULL);
#endif

	if (!rline) {
		return 0;
	}
	size_t len = strlen(rline);
	if (!(state->options & MRSH_OPT_NOLOG)) {
		add_history(rline);
		char *history_path = get_history_path();
		write_history(history_path);
		free(history_path);
	}
	*line = malloc(len + 2);
	strcpy(*line, rline);
	strcat(*line, "\n");
	free(rline);
	return len + 1;
}
