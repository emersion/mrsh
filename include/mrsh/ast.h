#ifndef _MRSH_AST_H
#define _MRSH_AST_H

#include <mrsh/array.h>
#include <stdbool.h>

/**
 * Position describes an arbitrary source position include line and column
 * location.
 */
struct mrsh_position {
	size_t offset; // starting at 0
	int line; // starting at 1
	int column; // starting at 1
};

enum mrsh_word_type {
	MRSH_WORD_STRING,
	MRSH_WORD_PARAMETER,
	MRSH_WORD_COMMAND,
	MRSH_WORD_LIST,
};

/**
 * A word can be:
 * - An unquoted or a single-quoted string
 * - A candidate for parameter expansion
 * - A candidate for command substitution
 * - An unquoted or a double-quoted list of words
 */
struct mrsh_word {
	enum mrsh_word_type type;
};

/**
 * A string word is a type of word. It can be unquoted or single-quoted.
 */
struct mrsh_word_string {
	struct mrsh_word word;
	struct mrsh_position begin, end;
	char *str;
	bool single_quoted;
};

enum mrsh_word_parameter_op {
	MRSH_PARAM_NONE, // No-op
	MRSH_PARAM_MINUS, // `${parameter:-[word]}`, Use Default Values
	MRSH_PARAM_EQUAL, // `${parameter:=[word]}`, Assign Default Values
	MRSH_PARAM_QMARK, // `${parameter:?[word]}`, Indicate Error if Null or Unset
	MRSH_PARAM_PLUS, // `${parameter:+[word]}`, Use Alternative Value
	MRSH_PARAM_LEADING_HASH, // `${#parameter}`, String Length
	MRSH_PARAM_PERCENT, // `${parameter%[word]}`, Remove Smallest Suffix Pattern
	MRSH_PARAM_DPERCENT, // `${parameter%%[word]}`, Remove Largest Suffix Pattern
	MRSH_PARAM_HASH, // `${parameter#[word]}`, Remove Smallest Prefix Pattern
	MRSH_PARAM_DHASH, // `${parameter##[word]}`, Remove Largest Prefix Pattern
};

/**
 * A word parameter is a type of word candidate for parameter expansion. The
 * format is either `$name` or `${expression}`.
 */
struct mrsh_word_parameter {
	struct mrsh_word word;
	char *name;
	enum mrsh_word_parameter_op op;
	bool colon; // only for -, =, ?, +
	struct mrsh_word *arg; // can be NULL
};

/**
 * A word command is a type of word candidate for command substitution. The
 * format is either `` `command` `` or `$(command)`.
 */
struct mrsh_word_command {
	struct mrsh_word word;
	struct mrsh_program *program; // can be NULL
	bool back_quoted;
};

/**
 * A word list is a type of word. It can be unquoted or double-quoted. Its
 * children are _not_ separated by blanks. Here's an example:
 *
 *   abc"d ef"g'h i'
 */
struct mrsh_word_list {
	struct mrsh_word word;
	struct mrsh_array children; // struct mrsh_word *
	bool double_quoted;
	struct mrsh_position lquote_pos, rquote_pos; // can be invalid
};

enum mrsh_io_redirect_op {
	MRSH_IO_LESS, // <
	MRSH_IO_GREAT, // >
	MRSH_IO_CLOBBER, // >|
	MRSH_IO_DGREAT, // >>
	MRSH_IO_LESSAND, // <&
	MRSH_IO_GREATAND, // >&
	MRSH_IO_LESSGREAT, // <>
	MRSH_IO_DLESS, // <<
	MRSH_IO_DLESSDASH, // <<-
};

/**
 * An IO redirection operator. The format is: `[io_number]op name`.
 */
struct mrsh_io_redirect {
	int io_number; // -1 if unspecified
	enum mrsh_io_redirect_op op;
	struct mrsh_word *name; // filename or here-document delimiter
	struct mrsh_array here_document; // struct mrsh_word *, only for << and <<-
};

/**
 * A variable assignment. The format is: `name=value`.
 */
struct mrsh_assignment {
	char *name;
	struct mrsh_word *value;
};

enum mrsh_command_type {
	MRSH_SIMPLE_COMMAND,
	MRSH_BRACE_GROUP,
	MRSH_IF_CLAUSE,
	MRSH_FUNCTION_DEFINITION,
};

/**
 * A command. It is either a simple command, a brace group or an if clause.
 */
struct mrsh_command {
	enum mrsh_command_type type;
};

/**
 * A simple command is a type of command. It contains a command name, followed
 * by command arguments. It can also contain IO redirections and variable
 * assignments.
 */
struct mrsh_simple_command {
	struct mrsh_command command;
	struct mrsh_word *name; // can be NULL if it contains only assignments
	struct mrsh_array arguments; // struct mrsh_word *
	struct mrsh_array io_redirects; // struct mrsh_io_redirect *
	struct mrsh_array assignments; // struct mrsh_assignment *
};

/**
 * A brace group is a type of command. It contains command lists and executes
 * them in the current process environment. The format is:
 * `{ compound-list ; }`.
 */
struct mrsh_brace_group {
	struct mrsh_command command;
	struct mrsh_array body; // struct mrsh_command_list *
};

/**
 * An if clause is a type of command. The format is:
 *
 *   if compound-list
 *   then
 *       compound-list
 *   [elif compound-list
 *   then
 *       compound-list] ...
 *   [else
 *       compound-list]
 *   fi
 */
struct mrsh_if_clause {
	struct mrsh_command command;
	struct mrsh_array condition; // struct mrsh_command_list *
	struct mrsh_array body; // struct mrsh_command_list *
	struct mrsh_command *else_part; // can be NULL
};

/**
 * A function definition is a type of command. The format is:
 *
 *   fname ( ) compound-command [io-redirect ...]
 */
struct mrsh_function_definition {
	struct mrsh_command command;
	char *name;
	struct mrsh_command *body;
};

enum mrsh_node_type {
	MRSH_NODE_PIPELINE,
	MRSH_NODE_BINOP,
};

/**
 * A node is an AND-OR list component. It is either a pipeline or a binary
 * operation.
 */
struct mrsh_node {
	enum mrsh_node_type type;
};

/**
 * A pipeline is a type of node which consists of multiple commands
 * separated by `|`. The format is: `[!] command1 [ | command2 ...]`.
 */
struct mrsh_pipeline {
	struct mrsh_node node;
	struct mrsh_array commands; // struct mrsh_command *
	bool bang; // whether the pipeline begins with `!`
};

enum mrsh_binop_type {
	MRSH_BINOP_AND, // `&&`
	MRSH_BINOP_OR, // `||`
};

/**
 * A binary operation is a type of node which consists of multiple pipelines
 * separated by `&&` or `||`.
 */
struct mrsh_binop {
	struct mrsh_node node;
	enum mrsh_binop_type type;
	struct mrsh_node *left, *right;
};

/**
 * A command list contains AND-OR lists separated by `;` (for sequential
 * execution) or `&` (for asynchronous execution).
 */
struct mrsh_command_list {
	struct mrsh_node *node;
	bool ampersand; // whether the command list ends with `&`
};

/**
 * A shell program. It contains command lists.
 */
struct mrsh_program {
	struct mrsh_array body; // struct mrsh_command_list *
};

bool mrsh_position_valid(const struct mrsh_position *pos);

void mrsh_word_destroy(struct mrsh_word *word);
void mrsh_io_redirect_destroy(struct mrsh_io_redirect *redir);
void mrsh_assignment_destroy(struct mrsh_assignment *assign);
void mrsh_command_destroy(struct mrsh_command *cmd);
void mrsh_node_destroy(struct mrsh_node *node);
void mrsh_command_list_destroy(struct mrsh_command_list *l);
void mrsh_program_destroy(struct mrsh_program *prog);
struct mrsh_word_string *mrsh_word_string_create(char *str,
	bool single_quoted);
struct mrsh_word_parameter *mrsh_word_parameter_create(char *name,
	enum mrsh_word_parameter_op op, bool colon, struct mrsh_word *arg);
struct mrsh_word_command *mrsh_word_command_create(struct mrsh_program *prog,
	bool back_quoted);
struct mrsh_word_list *mrsh_word_list_create(struct mrsh_array *children,
	bool double_quoted);
struct mrsh_word_string *mrsh_word_get_string(struct mrsh_word *word);
struct mrsh_word_parameter *mrsh_word_get_parameter(struct mrsh_word *word);
struct mrsh_word_command *mrsh_word_get_command(struct mrsh_word *word);
struct mrsh_word_list *mrsh_word_get_list(struct mrsh_word *word);
struct mrsh_simple_command *mrsh_simple_command_create(struct mrsh_word *name,
	struct mrsh_array *arguments, struct mrsh_array *io_redirects,
	struct mrsh_array *assignments);
struct mrsh_brace_group *mrsh_brace_group_create(struct mrsh_array *body);
struct mrsh_if_clause *mrsh_if_clause_create(struct mrsh_array *condition,
	struct mrsh_array *body, struct mrsh_command *else_part);
struct mrsh_function_definition *mrsh_function_definition_create(char *name,
	struct mrsh_command *body);
struct mrsh_simple_command *mrsh_command_get_simple_command(
	struct mrsh_command *cmd);
struct mrsh_brace_group *mrsh_command_get_brace_group(struct mrsh_command *cmd);
struct mrsh_if_clause *mrsh_command_get_if_clause(struct mrsh_command *cmd);
struct mrsh_function_definition *mrsh_command_get_function_definition(
	struct mrsh_command *cmd);

struct mrsh_pipeline *mrsh_pipeline_create(struct mrsh_array *commands,
	bool bang);
struct mrsh_binop *mrsh_binop_create(enum mrsh_binop_type type,
	struct mrsh_node *left, struct mrsh_node *right);
struct mrsh_pipeline *mrsh_node_get_pipeline(struct mrsh_node *node);
struct mrsh_binop *mrsh_node_get_binop(struct mrsh_node *node);

char *mrsh_word_str(struct mrsh_word *word);
void mrsh_program_print(struct mrsh_program *prog);

#endif
