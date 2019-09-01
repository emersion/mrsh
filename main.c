#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mrsh/ast.h>
#include <mrsh/buffer.h>
#include <mrsh/builtin.h>
#include <mrsh/entry.h>
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "frontend.h"

extern char **environ;

int main(int argc, char *argv[]) {
	struct mrsh_state state = {0};
	mrsh_state_init(&state);

	struct mrsh_init_args init_args = {0};
	if (mrsh_process_args(&state, &init_args, argc, argv) != 0) {
		mrsh_state_finish(&state);
		return 1;
	}

	if (!mrsh_populate_env(&state, environ)) {
		return 1;
	}

	if (!(state.options & MRSH_OPT_NOEXEC)) {
		// If argv[0] begins with `-`, it's a login shell
		if (state.args->argv[0][0] == '-') {
			mrsh_source_profile(&state);
		}
		if (state.interactive) {
			mrsh_source_env(&state);
		}
	}

	state.fd = -1;
	struct mrsh_buffer parser_buffer = {0};
	struct mrsh_parser *parser;
	if (state.interactive) {
		interactive_init(&state);
		parser = mrsh_parser_with_buffer(&parser_buffer);
		state.fd = STDIN_FILENO;
	} else {
		if (init_args.command_str) {
			parser = mrsh_parser_with_data(init_args.command_str,
				strlen(init_args.command_str));
		} else {
			int fd;
			if (init_args.command_file) {
				fd = open(init_args.command_file, O_RDONLY | O_CLOEXEC);
				if (fd < 0) {
					fprintf(stderr, "failed to open %s for reading: %s\n",
						init_args.command_file, strerror(errno));
					return 1;
				}
			} else {
				fd = STDIN_FILENO;
			}

			parser = mrsh_parser_with_fd(fd);
			state.fd = fd;
		}
	}
	mrsh_state_set_parser_alias_func(&state, parser);

	if (state.interactive) {
		if (!mrsh_set_job_control(&state, true)) {
			fprintf(stderr, "failed to enable job control\n");
		}
	}

	struct mrsh_buffer read_buffer = {0};
	while (state.exit == -1) {
		if (state.interactive) {
			char *prompt;
			if (read_buffer.len > 0) {
				prompt = mrsh_get_ps2(&state);
			} else {
				// TODO: next_history_id
				prompt = mrsh_get_ps1(&state, 0);
			}
			char *line = NULL;
			size_t n = interactive_next(&state, &line, prompt);
			free(prompt);
			if (!line) {
				state.exit = state.last_status;
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
					state.exit = 1;
					break;
				}
			} else if (mrsh_parser_eof(parser)) {
				state.exit = state.last_status;
				break;
			} else {
				fprintf(stderr, "unknown error\n");
				state.exit = 1;
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
	if (state.fd >= 0) {
		close(state.fd);
	}

	return state.exit;
}
