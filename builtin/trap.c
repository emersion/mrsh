#define _XOPEN_SOURCE 1 // for SIGPOLL and SIGVTALRM
#include <assert.h>
#include <mrsh/getopt.h>
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtin.h"
#include "shell/shell.h"
#include "shell/trap.h"

static const char trap_usage[] =
	"usage: trap <n> [condition...]\n"
	"       trap [action condition...]\n";

static const char *sig_names[] = {
	[SIGABRT] = "ABRT",
	[SIGALRM] = "ALRM",
	[SIGBUS] = "BUS",
	[SIGCHLD] = "CHLD",
	[SIGCONT] = "CONT",
	[SIGFPE] = "FPE",
	[SIGHUP] = "HUP",
	[SIGILL] = "ILL",
	[SIGINT] = "INT",
	[SIGKILL] = "KILL",
	[SIGPIPE] = "PIPE",
	[SIGQUIT] = "QUIT",
	[SIGSEGV] = "SEGV",
	[SIGSTOP] = "STOP",
	[SIGTERM] = "TERM",
	[SIGTSTP] = "TSTP",
	[SIGTTIN] = "TTIN",
	[SIGTTOU] = "TTOU",
	[SIGUSR1] = "USR1",
	[SIGUSR2] = "USR2",
	// Some BSDs have decided against implementing SIGPOLL for functional and
	// security reasons
#ifdef SIGPOLL
	[SIGPOLL] = "POLL",
#endif
	[SIGPROF] = "PROF",
	[SIGSYS] = "SYS",
	[SIGTRAP] = "TRAP",
	[SIGURG] = "URG",
	[SIGVTALRM] = "VTALRM",
	[SIGXCPU] = "XCPU",
	[SIGXFSZ] = "XFSZ",
};

static bool is_decimal_str(const char *str) {
	for (size_t i = 0; str[i] != '\0'; i++) {
		if (str[i] < '0' || str[i] > '9') {
			return false;
		}
	}
	return true;
}

static int parse_sig(const char *str) {
	if (strcmp(str, "0") == 0 || strcmp(str, "EXIT") == 0) {
		return 0;
	}

	// XSI-conformant systems need to recognize a few more numeric signal
	// numbers
	if (strcmp(str, "1") == 0) {
		return SIGHUP;
	} else if (strcmp(str, "2") == 0) {
		return SIGINT;
	} else if (strcmp(str, "3") == 0) {
		return SIGQUIT;
	} else if (strcmp(str, "6") == 0) {
		return SIGABRT;
	} else if (strcmp(str, "9") == 0) {
		return SIGKILL;
	} else if (strcmp(str, "14") == 0) {
		return SIGALRM;
	} else if (strcmp(str, "15") == 0) {
		return SIGTERM;
	}

	for (size_t i = 0; i < sizeof(sig_names) / sizeof(sig_names[0]); i++) {
		if (sig_names[i] == NULL) {
			continue;
		}
		if (strcmp(str, sig_names[i]) == 0) {
			return (int)i;
		}
	}

	fprintf(stderr, "trap: failed to parse condition: %s\n", str);
	return -1;
}

static const char *sig_str(int sig) {
	if (sig == 0) {
		return "EXIT";
	}

	assert(sig > 0);
	assert((size_t)sig < sizeof(sig_names) / sizeof(sig_names[0]));
	assert(sig_names[sig] != NULL);
	return sig_names[sig];
}

static void print_traps(struct mrsh_state *state) {
	struct mrsh_state_priv *priv = state_get_priv(state);
	for (size_t i = 0; i < sizeof(priv->traps) / sizeof(priv->traps[0]); i++) {
		struct mrsh_trap *trap = &priv->traps[i];
		if (!trap->set) {
			continue;
		}
		printf("trap -- ");
		switch (trap->action) {
		case MRSH_TRAP_DEFAULT:
			printf("-");
			break;
		case MRSH_TRAP_IGNORE:
			printf("''");
			break;
		case MRSH_TRAP_CATCH:;
			char *cmd = mrsh_node_format(&trap->program->node);
			print_escaped(cmd);
			free(cmd);
			break;
		}
		printf(" %s\n", sig_str(i));
	}
}

int builtin_trap(struct mrsh_state *state, int argc, char *argv[]) {
	mrsh_optind = 0;
	if (mrsh_getopt(argc, argv, ":") != -1) {
		fprintf(stderr, "trap: unknown option -- %c\n", mrsh_optopt);
		fprintf(stderr, trap_usage);
		return 1;
	}
	if (mrsh_optind == argc) {
		print_traps(state);
		return 0;
	}

	const char *action_str;
	if (is_decimal_str(argv[mrsh_optind])) {
		action_str = "-";
	} else {
		action_str = argv[mrsh_optind];
		mrsh_optind++;
	}

	enum mrsh_trap_action action;
	struct mrsh_program *program = NULL;
	if (action_str[0] == '\0') {
		action = MRSH_TRAP_IGNORE;
	} else if (strcmp(action_str, "-") == 0) {
		action = MRSH_TRAP_DEFAULT;
	} else {
		action = MRSH_TRAP_CATCH;

		struct mrsh_parser *parser =
			mrsh_parser_with_data(action_str, strlen(action_str));
		program = mrsh_parse_program(parser);
		if (program == NULL) {
			struct mrsh_position err_pos;
			const char *err_msg = mrsh_parser_error(parser, &err_pos);
			if (err_msg != NULL) {
				fprintf(stderr, "trap: %d:%d: %s\n",
					err_pos.line, err_pos.column, err_msg);
			} else {
				fprintf(stderr, "trap: unknown parse error\n");
			}
			mrsh_parser_destroy(parser);
			return 1;
		}
		mrsh_parser_destroy(parser);
	}

	for (int i = mrsh_optind; i < argc; i++) {
		int sig = parse_sig(argv[i]);
		if (sig < 0) {
			return 1;
		}
		if (sig == SIGKILL || sig == SIGSTOP) {
			fprintf(stderr, "trap: setting a trap for SIGKILL or SIGSTOP "
				"produces undefined results\n");
			return 1;
		}

		if (!set_trap(state, sig, action, program)) {
			return 1;
		}
	}

	return 0;
}
