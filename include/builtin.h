#ifndef BUILTIN_H
#define BUILTIN_H

struct mrsh_state;

typedef int (*mrsh_builtin_func_t)(struct mrsh_state *state,
	int argc, char *argv[]);

void print_escaped(const char *value);

int builtin_alias(struct mrsh_state *state, int argc, char *argv[]);
int builtin_cd(struct mrsh_state *state, int argc, char *argv[]);
int builtin_colon(struct mrsh_state *state, int argc, char *argv[]);
int builtin_dot(struct mrsh_state *state, int argc, char *argv[]);
int builtin_eval(struct mrsh_state *state, int argc, char *argv[]);
int builtin_exit(struct mrsh_state *state, int argc, char *argv[]);
int builtin_export(struct mrsh_state *state, int argc, char *argv[]);
int builtin_false(struct mrsh_state *state, int argc, char *argv[]);
int builtin_getopts(struct mrsh_state *state, int argc, char *argv[]);
int builtin_pwd(struct mrsh_state *state, int argc, char *argv[]);
int builtin_read(struct mrsh_state *state, int argc, char *argv[]);
int builtin_set(struct mrsh_state *state, int argc, char *argv[]);
int builtin_shift(struct mrsh_state *state, int argc, char *argv[]);
int builtin_times(struct mrsh_state *state, int argc, char *argv[]);
int builtin_true(struct mrsh_state *state, int argc, char *argv[]);
int builtin_type(struct mrsh_state *state, int argc, char *argv[]);
int builtin_umask(struct mrsh_state *state, int argc, char *argv[]);
int builtin_unalias(struct mrsh_state *state, int argc, char *argv[]);
int builtin_unset(struct mrsh_state *state, int argc, char *argv[]);

const char *print_options(struct mrsh_state *state);

#endif
