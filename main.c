#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mrsh/ast.h>
#include <mrsh/buffer.h>
#include <mrsh/builtin.h>
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "builtin.h"
#include "frontend.h"

static char *expand_ps(struct mrsh_state *state, const char *ps1) {
	struct mrsh_parser *parser = mrsh_parser_with_data(ps1, strlen(ps1));
	struct mrsh_word *word = mrsh_parse_word(parser);
	mrsh_parser_destroy(parser);
	if (word == NULL) {
		return NULL;
	}

	mrsh_run_word(state, &word);

	return mrsh_word_str(word);
}

static char *get_ps1(struct mrsh_state *state) {
	const char *ps1 = mrsh_env_get(state, "PS1", NULL);
	if (ps1 != NULL) {
		// TODO: Replace ! with next history ID
		return expand_ps(state, ps1);
	}
	char *p = malloc(3);
	sprintf(p, "%s", getuid() ? "$ " : "# ");
	return p;
}

static char *get_ps2(struct mrsh_state *state) {
	const char *ps2 = mrsh_env_get(state, "PS2", NULL);
	if (ps2 != NULL) {
		return expand_ps(state, ps2);
	}
	return strdup("> ");
}

static void source_profile(struct mrsh_state *state) {
	char path[PATH_MAX + 1];
	int n = snprintf(path, sizeof(path), "%s/.profile", getenv("HOME"));
	if (n == sizeof(path)) {
		fprintf(stderr, "Warning: $HOME/.profile is longer than PATH_MAX\n");
		return;
	}
	if (access(path, F_OK) == -1) {
		return;
	}
	char *profile_argv[2] = { ".", path };
	mrsh_run_builtin(state, 2, profile_argv);
}

static void source_env(struct mrsh_state *state) {
	char *path = getenv("ENV");
	if (path == NULL) {
		return;
	}
	// TODO: parameter expansion
	if (access(path, F_OK) == -1) {
		return;
	}
	char *env_argv[] = { ".", path };
	mrsh_run_builtin(state, sizeof(env_argv) / sizeof(env_argv[0]), env_argv);
}

static const char *get_alias(const char *name, void *data) {
	struct mrsh_state *state = data;
	return mrsh_hashtable_get(&state->aliases, name);
}

extern char **environ;

int main(int argc, char *argv[]) {
	struct mrsh_state state = {0};
	mrsh_state_init(&state);

	struct mrsh_init_args init_args = {0};
	if (mrsh_process_args(&state, &init_args, argc, argv) != EXIT_SUCCESS) {
		mrsh_state_finish(&state);
		return EXIT_FAILURE;
	}

	for (size_t i = 0; environ[i] != NULL; ++i) {
		char *eql = strchr(environ[i], '=');
		size_t klen = eql - environ[i];
		char *key = strndup(environ[i], klen);
		char *val = &eql[1];
		mrsh_env_set(&state, key, val, MRSH_VAR_ATTRIB_EXPORT);
		free(key);
	}

	mrsh_env_set(&state, "IFS", " \t\n", MRSH_VAR_ATTRIB_NONE);

	pid_t ppid = getppid();
	char ppid_str[24];
	snprintf(ppid_str, sizeof(ppid_str), "%d", ppid);
	mrsh_env_set(&state, "PPID", ppid_str, MRSH_VAR_ATTRIB_NONE);

	// TODO check if path is well-formed, has . or .., and handle symbolic links
	const char *pwd = mrsh_env_get(&state, "PWD", NULL);
	if (pwd == NULL || strlen(pwd) >= PATH_MAX) {
		char cwd[PATH_MAX];
		getcwd(cwd, PATH_MAX);
		mrsh_env_set(&state, "PWD", cwd, MRSH_VAR_ATTRIB_EXPORT);
	}

	mrsh_env_set(&state, "OPTIND", "1", MRSH_VAR_ATTRIB_NONE);

	if (state.interactive && !(state.options & MRSH_OPT_NOEXEC)) {
		// If argv[0] begins with `-`, it's a login shell
		if (state.args->argv[0][0] == '-') {
			source_profile(&state);
		}
		source_env(&state);
	}

	int fd = -1;
	struct mrsh_buffer parser_buffer = {0};
	struct mrsh_parser *parser;
	if (state.interactive) {
		interactive_init(&state);
		parser = mrsh_parser_with_buffer(&parser_buffer);
	} else {
		if (init_args.command_str) {
			parser = mrsh_parser_with_data(init_args.command_str,
				strlen(init_args.command_str));
		} else {
			if (init_args.command_file) {
				fd = open(init_args.command_file, O_RDONLY | O_CLOEXEC);
				if (fd < 0) {
					fprintf(stderr, "failed to open %s for reading: %s",
						init_args.command_file, strerror(errno));
					return EXIT_FAILURE;
				}
			} else {
				fd = STDIN_FILENO;
			}

			parser = mrsh_parser_with_fd(fd);
		}
	}
	mrsh_parser_set_alias(parser, get_alias, &state);

	struct mrsh_buffer read_buffer = {0};
	while (state.exit == -1) {
		if (state.interactive) {
			char *prompt;
			if (read_buffer.len > 0) {
				prompt = get_ps2(&state);
			} else {
				prompt = get_ps1(&state);
			}
			char *line = NULL;
			size_t n = interactive_next(&state, &line, prompt);
			free(prompt);
			if (!line) {
				state.exit = EXIT_FAILURE;
				continue;
			}
			mrsh_buffer_append(&read_buffer, line, n);
			free(line);

			parser_buffer.len = 0;
			mrsh_buffer_append(&parser_buffer,
				read_buffer.data, read_buffer.len);

			mrsh_parser_reset(parser);
		}

		struct mrsh_program *prog = mrsh_parse_line(parser);
		if (mrsh_parser_continuation_line(parser)) {
			// Nothing to see here
		} else if (prog == NULL) {
			struct mrsh_position err_pos;
			const char *err_msg = mrsh_parser_error(parser, &err_pos);
			if (err_msg != NULL) {
				mrsh_buffer_finish(&read_buffer);
				fprintf(stderr, "%s:%d:%d: syntax error: %s\n",
					state.args->argv[0], err_pos.line, err_pos.column, err_msg);
				if (state.interactive) {
					continue;
				} else {
					state.exit = EXIT_FAILURE;
					break;
				}
			} else if (mrsh_parser_eof(parser)) {
				state.exit = state.last_status;
				break;
			} else {
				fprintf(stderr, "unknown error\n");
				state.exit = EXIT_FAILURE;
				break;
			}
		} else {
			if ((state.options & MRSH_OPT_NOEXEC)) {
				mrsh_program_print(prog);
			} else {
				mrsh_run_program(&state, prog);
			}
			mrsh_buffer_finish(&read_buffer);
		}
		mrsh_program_destroy(prog);
	}

	if (state.interactive) {
		printf("\n");
	}

	mrsh_buffer_finish(&read_buffer);
	mrsh_parser_destroy(parser);
	mrsh_buffer_finish(&parser_buffer);
	mrsh_state_finish(&state);
	if (fd >= 0) {
		close(fd);
	}

	return state.exit;
}
