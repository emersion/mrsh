#ifndef MRSH_AST_H
#define MRSH_AST_H

#include <mrsh/array.h>
#include <stdbool.h>

/**
 * Position describes an arbitrary source position including line and column
 * location.
 */
struct mrsh_position {
	size_t offset; // starting at 0
	int line; // starting at 1
	int column; // starting at 1
};

/**
 * Range describes a continuous source region. It has a beginning position and
 * a non-included ending position.
 */
struct mrsh_range {
	struct mrsh_position begin, end;
};

enum mrsh_node_type {
	MRSH_NODE_PROGRAM,
	MRSH_NODE_COMMAND_LIST,
	MRSH_NODE_AND_OR_LIST,
	MRSH_NODE_COMMAND,
	MRSH_NODE_WORD,
};

struct mrsh_node {
	enum mrsh_node_type type;
};

enum mrsh_word_type {
	MRSH_WORD_STRING,
	MRSH_WORD_PARAMETER,
	MRSH_WORD_COMMAND,
	MRSH_WORD_ARITHMETIC,
	MRSH_WORD_LIST,
};

/**
 * A word can be:
 * - An unquoted or a single-quoted string
 * - A candidate for parameter expansion
 * - A candidate for command substitution
 * - A candidate for arithmetic expansion
 * - An unquoted or a double-quoted list of words
 */
struct mrsh_word {
	struct mrsh_node node;
	enum mrsh_word_type type;
};

/**
 * A string word is a type of word. It can be unquoted or single-quoted.
 */
struct mrsh_word_string {
	struct mrsh_word word;
	char *str;
	bool single_quoted;

	struct mrsh_range range;
};

enum mrsh_word_parameter_op {
	MRSH_PARAM_NONE, // `$name` or `${parameter}`, no-op
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

	struct mrsh_position dollar_pos;
	struct mrsh_range name_range;
	struct mrsh_range op_range; // can be invalid
	struct mrsh_position lbrace_pos, rbrace_pos; // can be invalid
};

/**
 * A word command is a type of word candidate for command substitution. The
 * format is either `` `command` `` or `$(command)`.
 */
struct mrsh_word_command {
	struct mrsh_word word;
	struct mrsh_program *program; // can be NULL
	bool back_quoted;

	struct mrsh_range range;
};

/**
 * An arithmetic word is a type of word containing an arithmetic expression. The
 * format is `$((expression))`.
 */
struct mrsh_word_arithmetic {
	struct mrsh_word word;
	struct mrsh_word *body;
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

	struct mrsh_position io_number_pos; // can be invalid
	struct mrsh_range op_range;
};

/**
 * A variable assignment. The format is: `name=value`.
 */
struct mrsh_assignment {
	char *name;
	struct mrsh_word *value;

	struct mrsh_range name_range;
	struct mrsh_position equal_pos;
};

enum mrsh_command_type {
	MRSH_SIMPLE_COMMAND,
	MRSH_BRACE_GROUP,
	MRSH_SUBSHELL,
	MRSH_IF_CLAUSE,
	MRSH_FOR_CLAUSE,
	MRSH_LOOP_CLAUSE, // `while` or `until`
	MRSH_CASE_CLAUSE,
	MRSH_FUNCTION_DEFINITION,
};

/**
 * A command. It is either a simple command, a brace group or an if clause.
 */
struct mrsh_command {
	struct mrsh_node node;
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

	struct mrsh_position lbrace_pos, rbrace_pos;
};

/**
 * A subshell is a type of command. It contains command lists and executes
 * them in a subshell environment. The format is: `( compound-list )`.
 */
struct mrsh_subshell {
	struct mrsh_command command;
	struct mrsh_array body; // struct mrsh_command_list *

	struct mrsh_position lparen_pos, rparen_pos;
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

	struct mrsh_range if_range; // for `if` or `elif`
	struct mrsh_range then_range, fi_range;
	struct mrsh_range else_range; // can be invalid
};

/**
 * A for clause is a type of command. The format is:
 *
 *   for name [ in [word ... ]]
 *   do
 *       compound-list
 *   done
 */
struct mrsh_for_clause {
	struct mrsh_command command;
	char *name;
	bool in;
	struct mrsh_array word_list; // struct mrsh_word *
	struct mrsh_array body; // struct mrsh_command_list *

	struct mrsh_range for_range, name_range, do_range, done_range;
	struct mrsh_range in_range; // can be invalid
};

enum mrsh_loop_type {
	MRSH_LOOP_WHILE,
	MRSH_LOOP_UNTIL,
};

/**
 * A loop clause is a type of command. The format is:
 *
 *   while/until compound-list-1
 *   do
 *       compound-list-2
 *   done
 */
struct mrsh_loop_clause {
	struct mrsh_command command;
	enum mrsh_loop_type type;
	struct mrsh_array condition; // struct mrsh_command_list *
	struct mrsh_array body; // struct mrsh_command_list *

	struct mrsh_range while_until_range; // for `while` or `until`
	struct mrsh_range do_range, done_range;
};

/**
 * A case item contains one or more patterns with a body. The format is:
 *
 *   [(] pattern[ | pattern] ... ) compound-list ;;
 *
 * The double-semicolumn is optional if it's the last item.
 */
struct mrsh_case_item {
	struct mrsh_array patterns; // struct mrsh_word *
	struct mrsh_array body; // struct mrsh_command_list *

	struct mrsh_position lparen_pos; // can be invalid
	// TODO: pipe positions between each pattern
	struct mrsh_position rparen_pos;
	struct mrsh_range dsemi_range; // can be invalid
};

/**
 * A case clause is a type of command. The format is:
 *
 *   case word in
 *       [(] pattern1 ) compound-list ;;
 *       [[(] pattern[ | pattern] ... ) compound-list ;;] ...
 *       [[(] pattern[ | pattern] ... ) compound-list]
 *   esac
 */
struct mrsh_case_clause {
	struct mrsh_command command;
	struct mrsh_word *word;
	struct mrsh_array items; // struct mrsh_case_item *

	struct mrsh_range case_range, in_range, esac_range;
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

	struct mrsh_range name_range;
	struct mrsh_position lparen_pos, rparen_pos;
};

enum mrsh_and_or_list_type {
	MRSH_AND_OR_LIST_PIPELINE,
	MRSH_AND_OR_LIST_BINOP,
};

/**
 * An AND-OR list is a tree of pipelines and binary operations.
 */
struct mrsh_and_or_list {
	struct mrsh_node node;
	enum mrsh_and_or_list_type type;
};

/**
 * A pipeline is a type of AND-OR list which consists of multiple commands
 * separated by `|`. The format is: `[!] command1 [ | command2 ...]`.
 */
struct mrsh_pipeline {
	struct mrsh_and_or_list and_or_list;
	struct mrsh_array commands; // struct mrsh_command *
	bool bang; // whether the pipeline begins with `!`

	struct mrsh_position bang_pos; // can be invalid
	// TODO: pipe positions between each command
};

enum mrsh_binop_type {
	MRSH_BINOP_AND, // `&&`
	MRSH_BINOP_OR, // `||`
};

/**
 * A binary operation is a type of AND-OR list which consists of multiple
 * pipelines separated by `&&` or `||`.
 */
struct mrsh_binop {
	struct mrsh_and_or_list and_or_list;
	enum mrsh_binop_type type;
	struct mrsh_and_or_list *left, *right;

	struct mrsh_range op_range;
};

/**
 * A command list contains AND-OR lists separated by `;` (for sequential
 * execution) or `&` (for asynchronous execution).
 */
struct mrsh_command_list {
	struct mrsh_node node;
	struct mrsh_and_or_list *and_or_list;
	bool ampersand; // whether the command list ends with `&`

	struct mrsh_position separator_pos; // can be invalid
};

/**
 * A shell program. It contains command lists.
 */
struct mrsh_program {
	struct mrsh_node node;
	struct mrsh_array body; // struct mrsh_command_list *
};

typedef void (*mrsh_node_iterator_func)(struct mrsh_node *node,
	void *user_data);

bool mrsh_position_valid(const struct mrsh_position *pos);
bool mrsh_range_valid(const struct mrsh_range *range);

struct mrsh_word_string *mrsh_word_string_create(char *str,
	bool single_quoted);
struct mrsh_word_parameter *mrsh_word_parameter_create(char *name,
	enum mrsh_word_parameter_op op, bool colon, struct mrsh_word *arg);
struct mrsh_word_command *mrsh_word_command_create(struct mrsh_program *prog,
	bool back_quoted);
struct mrsh_word_arithmetic *mrsh_word_arithmetic_create(
	struct mrsh_word *body);
struct mrsh_word_list *mrsh_word_list_create(struct mrsh_array *children,
	bool double_quoted);
struct mrsh_simple_command *mrsh_simple_command_create(struct mrsh_word *name,
	struct mrsh_array *arguments, struct mrsh_array *io_redirects,
	struct mrsh_array *assignments);
struct mrsh_brace_group *mrsh_brace_group_create(struct mrsh_array *body);
struct mrsh_subshell *mrsh_subshell_create(struct mrsh_array *body);
struct mrsh_if_clause *mrsh_if_clause_create(struct mrsh_array *condition,
	struct mrsh_array *body, struct mrsh_command *else_part);
struct mrsh_for_clause *mrsh_for_clause_create(char *name, bool in,
	struct mrsh_array *word_list, struct mrsh_array *body);
struct mrsh_loop_clause *mrsh_loop_clause_create(enum mrsh_loop_type type,
	struct mrsh_array *condition, struct mrsh_array *body);
struct mrsh_case_clause *mrsh_case_clause_create(struct mrsh_word *word,
	struct mrsh_array *items);
struct mrsh_function_definition *mrsh_function_definition_create(char *name,
	struct mrsh_command *body);
struct mrsh_pipeline *mrsh_pipeline_create(struct mrsh_array *commands,
	bool bang);
struct mrsh_binop *mrsh_binop_create(enum mrsh_binop_type type,
	struct mrsh_and_or_list *left, struct mrsh_and_or_list *right);
struct mrsh_command_list *mrsh_command_list_create(void);
struct mrsh_program *mrsh_program_create(void);

void mrsh_word_destroy(struct mrsh_word *word);
void mrsh_io_redirect_destroy(struct mrsh_io_redirect *redir);
void mrsh_assignment_destroy(struct mrsh_assignment *assign);
void mrsh_command_destroy(struct mrsh_command *cmd);
void mrsh_and_or_list_destroy(struct mrsh_and_or_list *and_or_list);
void mrsh_command_list_destroy(struct mrsh_command_list *l);
void mrsh_program_destroy(struct mrsh_program *prog);

struct mrsh_word *mrsh_node_get_word(struct mrsh_node *node);
struct mrsh_command *mrsh_node_get_command(struct mrsh_node *node);
struct mrsh_and_or_list *mrsh_node_get_and_or_list(struct mrsh_node *node);
struct mrsh_command_list *mrsh_node_get_command_list(struct mrsh_node *node);
struct mrsh_program *mrsh_node_get_program(struct mrsh_node *node);

struct mrsh_word_string *mrsh_word_get_string(const struct mrsh_word *word);
struct mrsh_word_parameter *mrsh_word_get_parameter(
	const struct mrsh_word *word);
struct mrsh_word_command *mrsh_word_get_command(const struct mrsh_word *word);
struct mrsh_word_arithmetic *mrsh_word_get_arithmetic(
	const struct mrsh_word *word);
struct mrsh_word_list *mrsh_word_get_list(const struct mrsh_word *word);

struct mrsh_simple_command *mrsh_command_get_simple_command(
	const struct mrsh_command *cmd);
struct mrsh_brace_group *mrsh_command_get_brace_group(
	const struct mrsh_command *cmd);
struct mrsh_subshell *mrsh_command_get_subshell(
	const struct mrsh_command *cmd);
struct mrsh_if_clause *mrsh_command_get_if_clause(
	const struct mrsh_command *cmd);
struct mrsh_for_clause *mrsh_command_get_for_clause(
	const struct mrsh_command *cmd);
struct mrsh_loop_clause *mrsh_command_get_loop_clause(
	const struct mrsh_command *cmd);
struct mrsh_case_clause *mrsh_command_get_case_clause(
	const struct mrsh_command *cmd);
struct mrsh_function_definition *mrsh_command_get_function_definition(
	const struct mrsh_command *cmd);

struct mrsh_pipeline *mrsh_and_or_list_get_pipeline(
	const struct mrsh_and_or_list *and_or_list);
struct mrsh_binop *mrsh_and_or_list_get_binop(
	const struct mrsh_and_or_list *and_or_list);

void mrsh_node_for_each(struct mrsh_node *node,
	mrsh_node_iterator_func iterator, void *user_data);

void mrsh_word_range(struct mrsh_word *word, struct mrsh_position *begin,
	struct mrsh_position *end);
void mrsh_command_range(struct mrsh_command *cmd, struct mrsh_position *begin,
	struct mrsh_position *end);
char *mrsh_word_str(struct mrsh_word *word);
void mrsh_program_print(struct mrsh_program *prog);

struct mrsh_word *mrsh_word_copy(const struct mrsh_word *word);
struct mrsh_io_redirect *mrsh_io_redirect_copy(
	const struct mrsh_io_redirect *redir);
struct mrsh_assignment *mrsh_assignment_copy(
	const struct mrsh_assignment *assign);
struct mrsh_command *mrsh_command_copy(const struct mrsh_command *cmd);
struct mrsh_and_or_list *mrsh_and_or_list_copy(
	const struct mrsh_and_or_list *and_or_list);
struct mrsh_command_list *mrsh_command_list_copy(
	const struct mrsh_command_list *l);
struct mrsh_program *mrsh_program_copy(const struct mrsh_program *prog);

#endif
